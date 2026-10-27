"""The game-data bug detective (DESIGN "Scope": side projects): reads Fallout: New Vegas' own files and lists
candidate bugs and cut content. Read-only; candidates, not verdicts: each one gets checked before it's called a bug.

Checks:
  links    records pointing at forms that don't exist, or were deleted (placed objects' bases, container and
           inventory items, levelled list entries, NPC templates, everything scripts refer to)
  stages   quest stages no script ever sets (literal SetStage calls in every script source in the game)
  actors   NPCs and creatures never placed, never in a levelled list, never a template, never named by a script

Usage: python tools/detective/detective.py [game Data folder]   (report: build/detective/report.md)
"""
import collections
import os
import re
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import esm  # noqa: E402
import logic  # noqa: E402

DEFAULT_DATA = 'E:/SteamLibrary/steamapps/common/Fallout New Vegas/Data'
LOAD_ORDER = ['FalloutNV.esm', 'DeadMoney.esm', 'HonestHearts.esm', 'OldWorldBlues.esm', 'LonesomeRoad.esm',
              'GunRunnersArsenal.esm', 'ClassicPack.esm', 'MercenaryPack.esm', 'TribalPack.esm', 'CaravanPack.esm']
RESERVED = 0x800  # form ids below this belong to the engine itself (the player, ...)

# (record type or '*', subrecord type) -> byte offset of a form id in it
LINK_FIELDS = {
    ('REFR', 'NAME'): 0, ('ACHR', 'NAME'): 0, ('ACRE', 'NAME'): 0, ('PGRE', 'NAME'): 0, ('PMIS', 'NAME'): 0,
    ('CONT', 'CNTO'): 0, ('NPC_', 'CNTO'): 0, ('CREA', 'CNTO'): 0,
    ('LVLI', 'LVLO'): 4, ('LVLC', 'LVLO'): 4, ('LVLN', 'LVLO'): 4,
    ('NPC_', 'TPLT'): 0, ('CREA', 'TPLT'): 0,
    ('*', 'SCRO'): 0,
    ('FLST', 'LNAM'): 0,   # form list entries
    ('CREA', 'CSCR'): 0,   # the creature whose sounds this one uses
    ('INFO', 'ANAM'): 0,   # a dialogue line's speaker
}
# Conditions (dialogue, AI, ...): parameters 1 and 2 and the reference they run on can be forms. Only counted as
# mentions (actors "used"), not checked as links: they can just as well be numbers.
CONDITION_FORM_OFFSETS = (12, 16, 24)  # parameter 1, parameter 2, reference (layout: logic.py)
COMPLETE_QUEST = re.compile(r'\bcompletequest\s+([A-Za-z0-9_]+)', re.IGNORECASE)
SET_STAGE = re.compile(r'\bsetstage\s+([A-Za-z0-9_]+)\s*[, ]\s*([A-Za-z0-9_.\-]+)', re.IGNORECASE)
TESTISH = re.compile(r'test|debug|^zz|deleted|unused|cut|^aaa', re.IGNORECASE)


def stage_info(record):
    """A quest's stage numbers, and the ones whose log entries complete the quest (QSDT flag 1)."""
    stages, completing, current = [], set(), None
    for sub_type, data in record.subrecords:
        if sub_type == 'INDX' and len(data) >= 2:
            current = struct.unpack_from('<h', data, 0)[0]
            stages.append(current)
        elif sub_type == 'QSDT' and data and current is not None and data[0] & 1:
            completing.add(current)
    return sorted(set(stages)), sorted(completing)


def links_of(load_order, plugin, record):
    """(field, load-order id) for each form id this record points at through LINK_FIELDS."""
    for sub_type, data in record.subrecords:
        offset = LINK_FIELDS.get((record.type, sub_type), LINK_FIELDS.get(('*', sub_type)))
        if offset is None or len(data) < offset + 4:
            continue
        local = struct.unpack_from('<I', data, offset)[0]
        if local == 0:
            continue
        yield sub_type, load_order.resolve(plugin, local), local


def script_texts(record):
    for sub_type, data in record.subrecords:
        if sub_type == 'SCTX':
            yield data.decode('cp1252', 'replace')


def describe(record):
    return f"{record.type} {record.form_id:08X} {record.editor_id() or '(no editor id)'} [{record.plugin}]"


def main(argv):
    data_dir = argv[0] if argv else DEFAULT_DATA
    started = time.time()
    paths = [os.path.join(data_dir, name) for name in LOAD_ORDER if os.path.exists(os.path.join(data_dir, name))]
    order = esm.LoadOrder(paths)
    winning = order.winning()
    by_editor_id = {}
    for form_id, (_, record) in winning.items():
        if record.editor_id():
            by_editor_id[record.editor_id().lower()] = record
    read_s = time.time() - started

    # links
    broken = []
    referenced = set()
    spoken_by = collections.Counter()   # actor -> dialogue lines naming it as the speaker
    asked_about = collections.Counter()  # actor -> dialogue lines whose conditions name it
    links_checked = 0
    for form_id, (plugin, record) in winning.items():
        if record.deleted:
            continue
        for field, target, local in links_of(order, plugin, record):
            if target is None:
                broken.append((record, field, local, 'its master file is missing'))
                continue
            if record.type == 'INFO' and field == 'ANAM':
                spoken_by[target] += 1  # (a speaker isn't a sign the actor is in the game)
            else:
                referenced.add(target)
            links_checked += 1
            if (target & 0x00FFFFFF) < RESERVED and (target >> 24) == 0:
                continue
            found = winning.get(target)
            if found is None:
                broken.append((record, field, target, 'no such form'))
            elif found[1].deleted:
                broken.append((record, field, target, 'deleted'))

    # stages
    set_literal = collections.defaultdict(set)
    set_by_variable = set()
    completed_by_script = set()  # quests some script finishes with CompleteQuest (not through a stage)
    for _, (_, record) in winning.items():
        if record.deleted:
            continue
        for text in script_texts(record):
            for line in text.splitlines():
                line = line.split(';', 1)[0]
                for quest in COMPLETE_QUEST.findall(line):
                    completed_by_script.add(quest.lower())
                for quest, stage in SET_STAGE.findall(line):
                    if stage.lstrip('-').isdigit():
                        set_literal[quest.lower()].add(int(stage))
                    else:
                        set_by_variable.add(quest.lower())
    unreachable = []
    for _, (_, record) in winning.items():
        if record.type != 'QUST' or record.deleted:
            continue
        name = record.editor_id().lower()
        if name in set_by_variable:
            continue
        stages, completing = stage_info(record)
        never = [s for s in stages if s not in set_literal.get(name, set())]
        if never and len(never) < len(stages):  # (quests with none set at all are listed apart)
            # The worrying case: every stage that completes the quest is one no script sets.
            stuck = bool(completing) and all(s in never for s in completing) and name not in completed_by_script
            unreachable.append((record, never, stages, completing, stuck))
    never_set_at_all = [r for _, (_, r) in winning.items() if r.type == 'QUST' and not r.deleted
                        and r.editor_id().lower() not in set_literal and r.editor_id().lower() not in set_by_variable
                        and any(t == 'INDX' for t, _ in r.subrecords)]

    # actors: conditions name them too. In dialogue that only means lines meant for them ("is it them?");
    # elsewhere (AI packages, quests) it counts as being used.
    for form_id, (plugin, record) in winning.items():
        if record.deleted:
            continue
        for sub_type, data in record.subrecords:
            if sub_type == 'CTDA':
                for offset in CONDITION_FORM_OFFSETS:
                    if len(data) >= offset + 4:
                        local = struct.unpack_from('<I', data, offset)[0]
                        if local and (local >> 24) <= len(plugin.masters):
                            target = order.resolve(plugin, local)
                            if record.type == 'INFO':
                                asked_about[target] += 1
                            else:
                                referenced.add(target)
    unused, talk_only = [], []
    for form_id, (_, record) in winning.items():
        if record.type in ('NPC_', 'CREA') and not record.deleted and form_id not in referenced:
            if spoken_by[form_id] or asked_about[form_id]:
                talk_only.append((record, spoken_by[form_id], asked_about[form_id]))
            else:
                unused.append(record)

    # logic: objectives that get stuck; conditions and script branches testing stages no script sets (not for
    # quests set in ways we can't follow, nor ones no script sets at all: those are cut, and so is all that tests them)
    script_lines = [(record, line.split(';', 1)[0]) for _, (_, record) in winning.items() if not record.deleted
                    for text in script_texts(record) for line in text.splitlines()]
    quests = {r.editor_id().lower(): r for _, (_, r) in winning.items()
              if r.type == 'QUST' and not r.deleted and r.editor_id()}
    unsure_names = set(set_by_variable) | {r.editor_id().lower() for r in never_set_at_all}
    reachable_by_id = {quests[name].form_id: stages for name, stages in set_literal.items() if name in quests}
    unsure_ids = {quests[name].form_id for name in unsure_names if name in quests}
    stuck_objectives, missing_objectives = logic.check_objectives(script_lines, quests)
    dead_conditions = logic.check_conditions(order, winning, reachable_by_id, unsure_ids)
    dead_branches = logic.check_script_branches(script_lines, quests, set_literal, unsure_names)
    checks = (stuck_objectives, missing_objectives, dead_conditions, dead_branches)

    write_report(order, winning, broken, unreachable, never_set_at_all, unused, talk_only, read_s,
                 time.time() - started, links_checked, checks)
    return 0


def full_name(record):
    data = record.first('FULL')
    return data.rstrip(b'\0').decode('cp1252', 'replace') if data else ''


def logic_section(checks):
    stuck, missing, dead, branches = checks
    lines = [f'## Quest objectives that get stuck ({len(stuck)} quests)', '',
             'Shown by a script, but never completed or hidden, in quests nothing completes all the objectives of '
             '(finishing a quest moves it out of the way, so the ones that matter are in quests that can stay open).',
             '']
    for record, indices in sorted(stuck, key=lambda s: s[0].editor_id().lower()):
        lines.append(f'- {describe(record)} "{full_name(record)}": objectives {indices}')
    lines += ['', f"## Scripts touching objectives the quest doesn't have ({len(missing)})", '']
    for record, quest, index, verb in sorted(missing, key=lambda m: (m[1].editor_id().lower(), m[2])):
        lines.append(f'- {describe(record)} {verb} objective {index} of {quest.editor_id()} '
                     f'(it has {sorted(logic.objectives_of(quest))})')
    by_quest = collections.defaultdict(list)
    for record, quest, what in dead:
        by_quest[quest.editor_id()].append((record, what))
    lines += ['', f'## Conditions that can never be true ({len(dead)}, in {len(by_quest)} quests)', '',
              'Records (dialogue lines, AI packages...) whose conditions test for a quest stage no script sets.', '']
    for quest_name, items in sorted(by_quest.items(), key=lambda q: -len(q[1])):
        kinds = collections.Counter(record.type for record, _ in items)
        tests = sorted({what for _, what in items})
        lines.append(f"- {quest_name}: {', '.join(f'{n} {t}' for t, n in kinds.most_common())}; tests {tests[:6]}"
                     f"{' ...' if len(tests) > 6 else ''}")
    lines += ['', f'## Script branches that can never run ({len(branches)})', '']
    for record, quest, what in sorted(branches, key=lambda b: (b[1].editor_id().lower(), b[2])):
        lines.append(f'- {describe(record)}: {what}')
    return lines + ['']


def write_report(order, winning, broken, unreachable, never_set, unused, talk_only, read_s, total_s,
                 links_checked, checks):
    out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                           'build', 'detective')
    os.makedirs(out_dir, exist_ok=True)
    lines = ['# Bug detective report', '',
             f"Plugins: {', '.join(p.name for p in order.plugins)}; {len(winning)} forms "
             f"(read in {read_s:.0f} s, checked in {total_s - read_s:.0f} s).", '']
    if order.missing_masters:
        lines += ['Missing masters: ' + ', '.join(f'{p} needs {m}' for p, m in order.missing_masters), '']

    lines += [f'## Broken links ({len(broken)})', '',
              'A record points at a form that doesn\'t exist in the load order, or was deleted.', '']
    for record, field, target, why in sorted(broken, key=lambda b: (b[0].plugin, b[0].type, b[0].form_id)):
        lines.append(f'- {describe(record)}: {field} -> {target:08X} ({why})')

    lines += ['', f'## Quest stages no script sets ({len(unreachable)} quests)', '',
              'Stages of quests that do have stages set by literal SetStage calls somewhere, but not these. Some '
              'are set in ways this check can\'t see (from the console, by another mod, by a variable).', '']
    stuck = [u for u in unreachable if u[4]]
    lines += [f'Quests whose every completing stage is one no script sets ({len(stuck)}): they may be impossible '
              "to finish, unless something this check can't see sets it.", '']
    for record, never, stages, completing, _ in sorted(stuck, key=lambda u: u[0].editor_id().lower()):
        lines.append(f"- **{describe(record)} \"{full_name(record)}\"**: completing stages {completing}, never set")
    lines += ['', 'All of them:', '']
    for record, never, stages, completing, _ in sorted(unreachable, key=lambda u: u[0].editor_id().lower()):
        lines.append(f"- {describe(record)} \"{full_name(record)}\": never set {never} (of {stages}; completing "
                     f"{completing})")
    lines += ['', f'Quests with stages, none set by any script ({len(never_set)}): ' +
              ', '.join(sorted(r.editor_id() for r in never_set)), '']

    lines += logic_section(checks)
    lines += [f'## Actors with dialogue who never appear ({len(talk_only)})', '',
              'Dialogue lines are meant for them (they speak, or the lines check for them), but nothing puts them '
              "in the game: cut characters with lines no one can hear, or used in ways this check can't see.", '']
    for record, spoken, asked in sorted(talk_only, key=lambda t: -(t[1] + t[2])):
        lines.append(f"- {describe(record)} \"{full_name(record)}\": speaks {spoken} lines, named by {asked} "
                     f"lines' conditions")
    lines.append('')
    testish = [r for r in unused if TESTISH.search(r.editor_id())]
    real = [r for r in unused if not TESTISH.search(r.editor_id())]
    lines += [f'## Actors nothing uses ({len(real)}, plus {len(testish)} named like tests)', '',
              'Never placed, never in a levelled list, never a template, never named by a script: cut content, '
              'or used in ways this check can\'t see yet.', '']
    for record in sorted(real, key=lambda r: (r.plugin, r.editor_id().lower())):
        lines.append(f"- {describe(record)} \"{full_name(record)}\"")
    path = os.path.join(out_dir, 'report.md')
    open(path, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    print(f'{len(winning)} forms; {links_checked} links checked; {len(broken)} broken links; {len(unreachable)} quests with stages no script sets '
          f'({len(never_set)} with none set); {len(talk_only)} actors with dialogue who never appear; '
          f'{len(real)} unused actors (+{len(testish)} test-named)')
    stuck, missing, dead, branches = checks
    print(f'{len(stuck)} quests with stuck objectives; {len(missing)} script calls on missing objectives; '
          f'{len(dead)} conditions that can never be true; {len(branches)} script branches that can never run')
    print(f'report: {path} ({total_s:.0f} s)')


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
