from pathlib import Path
import uuid

from perfetto.trace_builder.proto_builder import TraceProtoBuilder, TracePacket

from perfetto.protos.perfetto.trace.perfetto_trace_pb2 import (
    TrackEvent,
    TrackDescriptor,
    SourceLocation,
    LogMessage,
    LogMessageBody,
)

# See: https://www.robopenguins.com/making-a-tracing-profiler-pt1/#perfetto
# https://ui.perfetto.dev/
# https://perfetto.dev/docs/reference/trace-packet-proto
# https://perfetto.dev/docs/getting-started/converting#python-example
# https://perfetto.dev/docs/reference/synthetic-track-event


class PerfettoBuilder:
    _TRUSTED_PACKET_SEQUENCE_ID = 8009
    _PROCESS_NAME = "MinLoggerApp"
    _PROCESS_PID = 0

    def __init__(self) -> None:
        self.builder = TraceProtoBuilder()
        self.threads: dict[int, TrackDescriptor] = {}
        self.process_desc = None
        # Could actually cache data to avoid duplicate strings. To do this I
        # think it would need to be done upfront preprocessing all the messages
        # to generate the full interned state that would be used for all
        # subsequent packets:
        # https://perfetto.dev/docs/reference/synthetic-track-event#interning-data-for-trace-size-optimization
        self.internal_iids = 1
        self.start_time = 0

    def _get_timestamp_ns(self, timestamp: float):
        if self.start_time is None:
            self.start_time = timestamp

        return int((timestamp - self.start_time) * 1e9)

    def write_to_file(self, out_path: Path):
        with open(out_path, "wb") as f:
            f.write(self.builder.serialize())

    def add_log(self, timestamp: float, msg: str, thread_id: int, priority, file: Path, line: int):
        packet = self._add_slice_event(
            timestamp, TrackEvent.TYPE_INSTANT, thread_id, "log", file, line
        )
        body_id = self.internal_iids
        self.internal_iids += 1

        packet.interned_data.log_message_body.append(LogMessageBody(iid=body_id, body=msg))

        packet.track_event.log_message.prio = self._get_priority(priority)
        packet.track_event.log_message.source_location_iid = packet.track_event.source_location_iid
        packet.track_event.log_message.body_iid = body_id

    def enter_slice(self, timestamp: float, name: str, thread_id: int, file: Path, line: int):
        self._add_slice_event(timestamp, TrackEvent.TYPE_SLICE_BEGIN, thread_id, name, file, line)

    def exit_slice(self, timestamp: float, name: str, thread_id: int, file: Path, line: int):
        self._add_slice_event(timestamp, TrackEvent.TYPE_SLICE_END, thread_id, name, file, line)

    def set_thread_name(self, timestamp: float, thread_id: int, thread_name: str):
        thread_descr = self._get_or_create_thread_if_needed(timestamp, thread_id)
        thread_descr.thread.thread_name = thread_name

    def _add_source_location(self, packet: TracePacket, file: Path, line: int):
        source_id = self.internal_iids
        self.internal_iids += 1

        packet.interned_data.source_locations.append(
            SourceLocation(iid=source_id, file_name=str(file), line_number=line)
        )
        packet.track_event.source_location_iid = source_id

    @staticmethod
    def _get_priority(priority: str):
        LEVELS = {
            "DEBUG": LogMessage.PRIO_DEBUG,
            "INFO": LogMessage.PRIO_INFO,
            "WARN": LogMessage.PRIO_WARN,
            "ERROR": LogMessage.PRIO_ERROR,
            "CRITICAL": LogMessage.PRIO_FATAL,
        }
        return LEVELS.get(priority, LogMessage.PRIO_UNSPECIFIED)

    def _get_or_create_thread_if_needed(self, timestamp: float, thread_id: int) -> TrackDescriptor:
        if thread_id in self.threads:
            return self.threads[thread_id]
        else:
            proc_desc = self._get_or_create_process_if_needed(timestamp)
            packet = self.builder.add_packet()
            packet.timestamp = self._get_timestamp_ns(timestamp)
            desc = packet.track_descriptor
            desc.uuid = uuid.uuid4().int & ((1 << 63) - 1)
            desc.thread.pid = proc_desc.process.pid
            # Avoid thread id 0 since maps to swapper
            desc.thread.tid = thread_id + 1
            desc.thread.thread_name = f"thread_{thread_id}"
            self.threads[thread_id] = desc
            return desc

    def _get_or_create_process_if_needed(self, timestamp: float) -> TrackDescriptor:
        if self.process_desc is not None:
            return self.process_desc
        else:
            packet = self.builder.add_packet()
            packet.timestamp = self._get_timestamp_ns(timestamp)
            desc = packet.track_descriptor
            desc.uuid = uuid.uuid4().int & ((1 << 63) - 1)
            desc.process.pid = self._PROCESS_PID
            desc.process.process_name = self._PROCESS_NAME
            self.process_desc = desc
            return desc

    def _add_slice_event(
        self,
        timestamp: float,
        event_type: TrackEvent.Type,
        thread_id: int,
        name: str,
        file: Path,
        line: int,
    ) -> TracePacket:
        thread_descr = self._get_or_create_thread_if_needed(timestamp, thread_id)

        packet = self.builder.add_packet()
        packet.timestamp = self._get_timestamp_ns(timestamp)
        packet.track_event.type = event_type
        packet.track_event.track_uuid = thread_descr.uuid
        packet.track_event.name = name
        packet.trusted_packet_sequence_id = self._TRUSTED_PACKET_SEQUENCE_ID
        packet.sequence_flags = (
            TracePacket.SEQ_NEEDS_INCREMENTAL_STATE | TracePacket.SEQ_INCREMENTAL_STATE_CLEARED
        )
        self._add_source_location(packet, file, line)
        return packet
