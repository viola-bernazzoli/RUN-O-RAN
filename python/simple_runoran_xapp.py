#!/usr/bin/env python3

"""Minimal ORAN SRS xApp.

This xApp subscribes to the SRS and timing advance indications exposed by the
local service model and only reconstructs the per-UE, per-antenna message
stream. It intentionally avoids location estimation, control loops, and other
domain-specific logic so it can act as a base for downstream JCASP apps.
"""

import argparse
import logging
import os
import signal
import sys
from collections import defaultdict
from typing import Any, Dict, List, Tuple

from lib.xAppBase import xAppBase
from lib import srs_messages_pb2


XAPP_LOG_PATH = os.environ.get("XAPP_LOG_PATH", "/tmp/simple_runoran_xapp.log")


def configure_xapp_logging() -> None:
    formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
    file_handler = logging.FileHandler(XAPP_LOG_PATH, mode="w", encoding="utf-8")
    file_handler.setFormatter(formatter)

    root_logger = logging.getLogger()
    root_logger.handlers.clear()
    root_logger.setLevel(logging.INFO)
    root_logger.addHandler(file_handler)


configure_xapp_logging()
logger = logging.getLogger(__name__)
_STD_LOG_STREAM = None


def redirect_std_streams_to_log() -> None:
    global _STD_LOG_STREAM
    if _STD_LOG_STREAM is None:
        _STD_LOG_STREAM = open(XAPP_LOG_PATH, "a", buffering=1, encoding="utf-8")
        sys.stdout = _STD_LOG_STREAM
        sys.stderr = _STD_LOG_STREAM


class SimpleRunOranXapp(xAppBase):
    """Minimal SRS/TA aggregator with no location-specific logic."""

    SRS_RAN_FUNCTION_ID = 5
    SUBSCRIPTION_REPORT_PERIOD_MS = 20

    def __init__(self, config: str, http_server_port: int, rmr_port: int, num_rx_antennas: int = 2):
        super(SimpleRunOranXapp, self).__init__(config, http_server_port, rmr_port)
        self.expected_rx_antennas = max(1, int(num_rx_antennas))
        self.generated_srs_cache: Dict[int, Dict[int, List[complex]]] = defaultdict(dict)
        self.pending_received_srs: Dict[Tuple[int, int], Dict[int, List[complex]]] = defaultdict(dict)
        self.completed_srs_messages: List[Dict[str, Any]] = []
        self.latest_timing_advance: Dict[int, Dict[str, Any]] = {}

        logger.info("SimpleRunOranXapp initialized")
        logger.info("Expected RX antennas: %s", self.expected_rx_antennas)

    @staticmethod
    def _unpack_iq_samples(packed_samples):
        scale = 8192.0
        decoded = []
        for packed in packed_samples:
            packed = int(packed)
            real_q = (packed >> 16) & 0xFFFF
            imag_q = packed & 0xFFFF
            if real_q & 0x8000:
                real_q -= 0x10000
            if imag_q & 0x8000:
                imag_q -= 0x10000
            decoded.append(complex(float(real_q) / scale, float(imag_q) / scale))
        return decoded

    def parse_timing_advance_message(self, indication_msg):
        """Parse protobuf timing advance indication message."""
        try:
            ta_indication = srs_messages_pb2.TimingAdvanceIndicationMessage()
            ta_indication.ParseFromString(indication_msg)

            ta_observations = []
            for ta_data in ta_indication.timing_advance_data:
                source_name = srs_messages_pb2.TimingAdvanceSource.Name(ta_data.source)
                identifier = ta_data.rnti if ta_data.rnti else ta_data.ue_index
                ta_observations.append(
                    {
                        "rnti": identifier,
                        "ue_index": ta_data.ue_index,
                        "timestamp": ta_data.timestamp,
                        "timing_advance": ta_data.timing_advance,
                        "source": source_name,
                        "attached": ta_data.attached,
                    }
                )

            return ta_observations
        except Exception as exc:
            logger.error("Failed to parse timing advance message: %s", exc)
            return []

    def parse_srs_message(self, indication_msg):
        """Parse protobuf SRS indication message."""
        # #### Here add code for custom SRS parsing or preprocessing ####
        try:
            srs_indication = srs_messages_pb2.SrsIndicationMessage()
            srs_indication.ParseFromString(indication_msg)

            ue_srs_data = []
            for ue_data in srs_indication.ue_srs_data:
                antenna_data_list = []
                for ant_data in ue_data.antenna_data:
                    antenna_data_list.append(
                        {
                            "antenna_id": ant_data.antenna_id,
                            "generated": bool(ant_data.generated),
                            "srs": self._unpack_iq_samples(ant_data.srs.iq_samples),
                        }
                    )

                ue_srs_data.append(
                    {
                        "rnti": ue_data.rnti,
                        "timestamp": ue_data.timestamp,
                        "antenna_data": antenna_data_list,
                    }
                )

            return ue_srs_data
        except Exception as exc:
            logger.error("Failed to parse SRS message: %s", exc)
            return []

    def _store_timing_advance(self, observation: Dict[str, Any]) -> None:
        self.latest_timing_advance[int(observation["rnti"])] = observation

    def _store_generated_srs(self, rnti: int, antenna_id: int, samples: List[complex]) -> None:
        self.generated_srs_cache[int(rnti)][int(antenna_id)] = samples

    def _try_assemble_snapshot(self, rnti: int, timestamp: int):
        # #### Here add logic for custom multi-antenna snapshot handling ####
        key = (int(rnti), int(timestamp))
        received_antennas = self.pending_received_srs.get(key)
        if not received_antennas:
            return None

        if len(received_antennas) < self.expected_rx_antennas:
            return None

        candidate_antennas = sorted(received_antennas.keys())[: self.expected_rx_antennas]
        generated_by_antenna = self.generated_srs_cache.get(int(rnti), {})
        if any(generated_by_antenna.get(antenna_id) is None for antenna_id in candidate_antennas):
            return None

        antenna_data = []
        for antenna_id in candidate_antennas:
            antenna_data.append(
                {
                    "antenna_id": antenna_id,
                    "generated_srs": generated_by_antenna[antenna_id],
                    "received_srs": received_antennas[antenna_id],
                }
            )

        combined = {
            "rnti": int(rnti),
            "timestamp": int(timestamp),
            "antenna_data": antenna_data,
            "timing_advance": self.latest_timing_advance.get(int(rnti)),
        }
        self.completed_srs_messages.append(combined)
        del self.pending_received_srs[key]
        return combined

    def timing_advance_indication_callback(self, e2_agent_id, subscription_id, indication_hdr, indication_msg):
        observations = self.parse_timing_advance_message(indication_msg)
        for observation in observations:
            self._store_timing_advance(observation)
        logger.info(
            "Received %s timing advance observations from %s (subscription %s)",
            len(observations),
            e2_agent_id,
            subscription_id,
        )

    def srs_indication_callback(self, e2_agent_id, subscription_id, indication_hdr, indication_msg):
        # #### Here add logic for custom SRS-based processing or downstream callbacks ####
        ue_srs_data_list = self.parse_srs_message(indication_msg)
        assembled_messages = []

        for ue_data in ue_srs_data_list:
            rnti = int(ue_data["rnti"])
            timestamp = int(ue_data["timestamp"])

            for antenna_data in ue_data["antenna_data"]:
                antenna_id = int(antenna_data["antenna_id"])
                samples = list(antenna_data["srs"])

                if antenna_data["generated"]:
                    self._store_generated_srs(rnti, antenna_id, samples)
                    continue

                self.pending_received_srs[(rnti, timestamp)][antenna_id] = samples
                combined = self._try_assemble_snapshot(rnti, timestamp)
                if combined is not None:
                    assembled_messages.append(combined)

        if assembled_messages:
            logger.info(
                "Assembled %s complete SRS snapshots from %s (subscription %s)",
                len(assembled_messages),
                e2_agent_id,
                subscription_id,
            )
            for message in assembled_messages:
                logger.info(
                    "UE %s ts=%s antennas=%s ta=%s",
                    message["rnti"],
                    message["timestamp"],
                    len(message["antenna_data"]),
                    message["timing_advance"],
                )
        else:
            logger.info(
                "Received %s SRS UE records from %s (subscription %s)",
                len(ue_srs_data_list),
                e2_agent_id,
                subscription_id,
            )

    def subscribe_timing_advance_indication(self, e2_node_id):
        event_trigger_def = bytes([self.SUBSCRIPTION_REPORT_PERIOD_MS])
        action_def = bytes([1])
        return self.subscribe(
            e2_node_id,
            self.SRS_RAN_FUNCTION_ID,
            event_trigger_def,
            action_def,
            self.timing_advance_indication_callback,
        )

    def subscribe_srs_indication(self, e2_node_id):
        event_trigger_def = bytes([self.SUBSCRIPTION_REPORT_PERIOD_MS])
        action_def = bytes([2])
        return self.subscribe(
            e2_node_id,
            self.SRS_RAN_FUNCTION_ID,
            event_trigger_def,
            action_def,
            self.srs_indication_callback,
        )

    @xAppBase.start_function
    def start(self, e2_node_id):
        # #### Here add logic for extra subscriptions or startup hooks ####
        logger.info("Starting minimal SRS xApp for E2 node %s", e2_node_id)
        ta_subscription_id = self.subscribe_timing_advance_indication(e2_node_id)
        srs_subscription_id = self.subscribe_srs_indication(e2_node_id)
        logger.info(
            "Subscriptions ready: timing advance=%s, srs=%s",
            ta_subscription_id,
            srs_subscription_id,
        )


def signal_handler(sig, frame):
    logger.info("Shutting down simple_runoran_xapp")
    sys.exit(0)


def main():
    parser = argparse.ArgumentParser(description="Minimal ORAN SRS xApp")
    parser.add_argument("--config", type=str, default="config.json", help="xApp configuration file")
    parser.add_argument("--http-port", type=int, default=8080, help="HTTP server port")
    parser.add_argument("--rmr-port", type=int, default=4560, help="RMR port")
    parser.add_argument("--e2-node-id", type=str, required=True, help="E2 node ID to subscribe to")
    parser.add_argument(
        "--num-rx-antennas",
        type=int,
        default=2,
        help="Number of RX antennas expected in one reconstructed SRS snapshot",
    )

    args = parser.parse_args()
    redirect_std_streams_to_log()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    xapp = SimpleRunOranXapp(args.config, args.http_port, args.rmr_port, args.num_rx_antennas)
    xapp.start(args.e2_node_id)


if __name__ == "__main__":
    main()
