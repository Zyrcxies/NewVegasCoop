"""The detective's logic checks: quest objectives that get stuck, and conditions (in records and in scripts) that
test for quest stages no script ever sets, so they can never be true."""
import collections
import re
import struct

# Condition function numbers: the commands' opcodes less 0x1000 (JohnnyGuitar internal/CommandOpcodes.h:
# GetStage 0x103A, GetStageDone 0x103B).
GET_STAGE = 0x3A
GET_STAGE_DONE = 0x3B

# A condition (CTDA, 28 bytes): type (top 3 bits: comparison; 0x04: compare with a global), 3 unused, the value
# to compare with (float, or a global's id), function (16 bits), 2 unused, parameter 1, parameter 2, run-on, ref.
CONDITION_SIZE = 28
COMPARISONS = ['==', '!=', '>', '>=', '<', '<=']
USES_GLOBAL = 0x04

OBJ_DISPLAYED = re.compile(r'\bsetobjectivedisplayed\s+([A-Za-z0-9_]+)\s*[, ]\s*(-?\d+)(?:\s*[, ]\s*(-?\d+))?',
                           re.IGNORECASE)
OBJ_COMPLETED = re.compile(r'\bsetobjectivecompleted\s+([A-Za-z0-9_]+)\s*[, ]\s*(-?\d+)(?:\s*[, ]\s*(-?\d+))?',
                           re.IGNORECASE)
COMPLETE_ALL = re.compile(r'\bcompleteallobjectives\s+([A-Za-z0-9_]+)', re.IGNORECASE)
SCRIPT_GET_STAGE = re.compile(r'\bgetstage\s+([A-Za-z0-9_]+)\s*(==|!=|>=|<=|>|<)\s*(-?\d+)', re.IGNORECASE)
SCRIPT_GET_STAGE_DONE = re.compile(r'\bgetstagedone\s+([A-Za-z0-9_]+)\s*[, ]\s*(-?\d+)(\s*==\s*0)?', re.IGNORECASE)


def compare(left, op, right):
    return {'==': left == right, '!=': left != right, '>': left > right, '>=': left >= right,
            '<': left < right, '<=': left <= right}[op]


def can_be_true(stages, op, value):
    """Can "GetStage == value" (or another comparison) ever hold? Before any stage is set, GetStage is 0."""
    return any(compare(s, op, value) for s in set(stages) | {0})


def objectives_of(quest):
    return {struct.unpack_from('<i', d, 0)[0] for t, d in quest.subrecords if t == 'QOBJ' and len(d) >= 4}


def check_objectives(script_lines, quests):
    """quests: editor id (lower case) -> QUST record. Returns (stuck, missing): objectives shown but never completed
    or hidden in quests nothing completes all of, and script calls on objectives the quest doesn't have."""
    shown, done, hidden = (collections.defaultdict(set) for _ in range(3))
    complete_all = set()
    missing = []
    for record, line in script_lines:
        for quest in COMPLETE_ALL.findall(line):
            complete_all.add(quest.lower())
        for pattern, verb in ((OBJ_DISPLAYED, 'shows'), (OBJ_COMPLETED, 'completes')):
            for quest, index, flag in pattern.findall(line):
                name, index = quest.lower(), int(index)
                on = flag.strip() != '0'
                if verb == 'shows':
                    (shown if on else hidden)[name].add(index)
                elif on:
                    done[name].add(index)
                quest_record = quests.get(name)
                if quest_record is not None and index not in objectives_of(quest_record):
                    missing.append((record, quest_record, index, verb if on else verb + ' (off)'))
    stuck = []
    for name, record in quests.items():
        if name in complete_all:
            continue
        never_done = sorted(shown[name] - done[name] - hidden[name])
        if never_done:
            stuck.append((record, never_done))
    return stuck, missing


def check_conditions(order, winning, reachable, unsure):
    """Conditions in records that test GetStage / GetStageDone against stages no script sets. reachable: quest form id
    -> stages scripts set; unsure: quest form ids set in ways we can't follow (skipped). Returns a list of
    (record, quest record, description)."""
    dead = []
    for form_id, (plugin, record) in winning.items():
        if record.deleted:
            continue
        for sub_type, data in record.subrecords:
            if sub_type != 'CTDA' or len(data) < CONDITION_SIZE:
                continue
            kind = data[0]
            if kind & USES_GLOBAL:
                continue
            op = COMPARISONS[(kind >> 5) % len(COMPARISONS)]
            value = struct.unpack_from('<f', data, 4)[0]
            function, param1, param2 = struct.unpack_from('<H2xII', data, 8)
            if function not in (GET_STAGE, GET_STAGE_DONE) or not param1:
                continue
            quest_id = order.resolve(plugin, param1)
            quest = winning.get(quest_id)
            if quest is None or quest[1].type != 'QUST' or quest_id in unsure:
                continue
            stages = reachable.get(quest_id, set())
            if function == GET_STAGE and not can_be_true(stages, op, value):
                dead.append((record, quest[1], f'GetStage {op} {value:g}'))
            elif function == GET_STAGE_DONE and param2 not in stages and compare(1, op, value) and not compare(0, op, value):
                dead.append((record, quest[1], f'GetStageDone {param2} {op} {value:g}'))
    return dead


def check_script_branches(script_lines, quests, reachable_by_name, unsure_names):
    """'if GetStage Q == N' / 'if GetStageDone Q N' in scripts, for stages no script sets."""
    dead = []
    for record, line in script_lines:
        for quest, op, value in SCRIPT_GET_STAGE.findall(line):
            name = quest.lower()
            if name in quests and name not in unsure_names and not can_be_true(reachable_by_name.get(name, set()), op, int(value)):
                dead.append((record, quests[name], f'GetStage {quest} {op} {value}'))
        for quest, stage, negated in SCRIPT_GET_STAGE_DONE.findall(line):
            name = quest.lower()
            if name in quests and name not in unsure_names and not negated and int(stage) not in reachable_by_name.get(name, set()):
                dead.append((record, quests[name], f'GetStageDone {quest} {stage}'))
    return dead
