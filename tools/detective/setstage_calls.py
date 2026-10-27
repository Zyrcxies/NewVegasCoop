"""Lists SetStage calls whose first word isn't a quest's editor id (a variable, a quest alias...), with the record
they're in: what the detective's stage check can't follow. Usage: python tools/detective/setstage_calls.py"""
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import detective  # noqa: E402
import esm  # noqa: E402


def main():
    paths = [os.path.join(detective.DEFAULT_DATA, n) for n in detective.LOAD_ORDER]
    order = esm.LoadOrder([p for p in paths if os.path.exists(p)])
    winning = order.winning()
    quests = {r.editor_id().lower() for _, r in winning.values() if r.type == 'QUST'}
    seen = collections.Counter()
    for _, record in winning.values():
        for text in detective.script_texts(record):
            for line in text.splitlines():
                line = line.split(';', 1)[0]
                for quest, stage in detective.SET_STAGE.findall(line):
                    if quest.lower() not in quests:
                        seen[(quest, stage, record.type, record.editor_id())] += 1
    for (quest, stage, rec_type, editor_id), count in seen.most_common(40):
        print(f'SetStage {quest} {stage}  x{count}  in {rec_type} {editor_id}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
