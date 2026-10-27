"""Lists every record and field that mentions a form id anywhere (any 4-byte position): to find kinds of
reference the detective's checks don't follow yet. Usage: python tools/detective/who_uses.py 0100D7EB [...]"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import detective  # noqa: E402
import esm  # noqa: E402


def main(argv):
    wanted = {int(a, 16) for a in argv}
    paths = [os.path.join(detective.DEFAULT_DATA, n) for n in detective.LOAD_ORDER]
    order = esm.LoadOrder([p for p in paths if os.path.exists(p)])
    for plugin, record in order.records():
        for sub_type, data in record.subrecords:
            for offset in range(0, len(data) - 3):
                local = struct.unpack_from('<I', data, offset)[0]
                if local and (local >> 24) <= len(plugin.masters):
                    target = order.resolve(plugin, local)
                    if target in wanted:
                        print(f'{target:08X} <- {record.type} {record.form_id:08X} {record.editor_id()} '
                              f'[{plugin.name}] {sub_type}+{offset}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
