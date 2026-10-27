"""Reads Fallout: New Vegas plugin files (.esm/.esp): records, their subrecords, and form ids translated to the
load order (a plugin's own ids say which of its masters they belong to). Read-only; nothing is written back.

Format (same as Fallout 3): records have a 24-byte header (type, data size, flags, form id, ...); groups ("GRUP")
hold records and other groups; record data is a list of subrecords (type, 16-bit size, data), with "XXXX" giving a
32-bit size for the next one; flag 0x00040000 means the data is zlib-compressed after a 4-byte size.
"""
import os
import struct
import zlib

FLAG_DELETED = 0x20
FLAG_COMPRESSED = 0x00040000


class Record:
    __slots__ = ('type', 'flags', 'form_id', 'plugin', 'subrecords', 'local_id')

    def __init__(self, type_, flags, form_id, plugin, subrecords, local_id):
        self.type = type_
        self.flags = flags
        self.form_id = form_id        # in the load order
        self.plugin = plugin          # file name it came from
        self.subrecords = subrecords  # list of (type, bytes)
        self.local_id = local_id      # as written in the plugin

    @property
    def deleted(self):
        return bool(self.flags & FLAG_DELETED)

    def first(self, sub_type):
        for t, data in self.subrecords:
            if t == sub_type:
                return data
        return None

    def editor_id(self):
        data = self.first('EDID')
        return data.rstrip(b'\0').decode('cp1252', 'replace') if data else ''


def read_subrecords(data):
    out = []
    pos = 0
    big = None
    while pos + 6 <= len(data):
        sub_type = data[pos:pos + 4].decode('ascii', 'replace')
        size = struct.unpack_from('<H', data, pos + 4)[0]
        pos += 6
        if big is not None:
            size, big = big, None
        if sub_type == 'XXXX':
            big = struct.unpack_from('<I', data, pos)[0]
            pos += size
            continue
        out.append((sub_type, data[pos:pos + size]))
        pos += size
    return out


class Plugin:
    def __init__(self, path):
        self.path = path
        self.name = os.path.basename(path)
        self.masters = []   # file names, in the plugin's own order
        self.records = []   # Record, with local ids until resolve()
        self._read()

    def _read(self):
        data = open(self.path, 'rb').read()
        if data[:4] != b'TES4':
            raise ValueError(f'{self.path}: not a plugin')
        header_size = struct.unpack_from('<I', data, 4)[0]
        for sub_type, sub in read_subrecords(data[24:24 + header_size]):
            if sub_type == 'MAST':
                self.masters.append(sub.rstrip(b'\0').decode('cp1252'))
        self._walk(data, 24 + header_size, len(data))

    def _walk(self, data, pos, end):
        while pos < end:
            rec_type = data[pos:pos + 4].decode('ascii', 'replace')
            size = struct.unpack_from('<I', data, pos + 4)[0]
            if rec_type == 'GRUP':
                self._walk(data, pos + 24, pos + size)
                pos += size
                continue
            flags, local_id = struct.unpack_from('<II', data, pos + 8)
            body = data[pos + 24:pos + 24 + size]
            if flags & FLAG_COMPRESSED and len(body) >= 4:
                try:
                    body = zlib.decompress(body[4:])
                except zlib.error:
                    body = b''
            self.records.append(Record(rec_type, flags, local_id, self.name, read_subrecords(body), local_id))
            pos += 24 + size


class LoadOrder:
    """Plugins in load order, with every form id translated to it."""

    def __init__(self, paths):
        self.plugins = [Plugin(p) for p in paths]
        self.index = {p.name.lower(): i for i, p in enumerate(self.plugins)}
        self.missing_masters = []
        for plugin in self.plugins:
            for master in plugin.masters:
                if master.lower() not in self.index:
                    self.missing_masters.append((plugin.name, master))
            for record in plugin.records:
                record.form_id = self.resolve(plugin, record.local_id)

    def resolve(self, plugin, local_id):
        """A plugin's own form id in the load order (top byte: which of its masters, or itself)."""
        top = local_id >> 24
        if top < len(plugin.masters):
            owner = self.index.get(plugin.masters[top].lower())
            if owner is None:
                return None
        else:
            owner = self.index[plugin.name.lower()]
        return (owner << 24) | (local_id & 0x00FFFFFF)

    def records(self):
        for plugin in self.plugins:
            for record in plugin.records:
                yield plugin, record

    def winning(self):
        """The last version of each form (later plugins override earlier ones), by load-order id."""
        out = {}
        for plugin, record in self.records():
            out[record.form_id] = (plugin, record)
        return out
