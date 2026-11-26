"""Generate auditable map reference tables. Never executes map code."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / 'Analysis' / 'Warlock1.00b CN'
DOCS = ROOT / 'Docs'
DOCS.mkdir(exist_ok=True)
SCRIPT = RAW / 'Scripts' / 'war3map.readable.j'
LINES = SCRIPT.read_text(encoding='utf-8').splitlines()
FUNCTIONS = {}
for i, line in enumerate(LINES):
    if line.startswith('function '):
        name = line.split()[1]
        end = next((j for j in range(i + 1, len(LINES)) if LINES[j] == 'endfunction'), len(LINES) - 1)
        FUNCTIONS.setdefault(name, []).append({'line': i + 1, 'end_line': end + 1})


def source(fn):
    entry = FUNCTIONS[fn][0]
    return f"[{fn}](../Analysis/Warlock1.00b%20CN/Scripts/war3map.readable.j#L{entry['line']})（第 {entry['line']} 行）"


def clean(value):
    return re.sub(r'\|c[0-9a-fA-F]{8}|\|r', '', str(value)).replace('|n', '；').replace('\r', '').replace('\n', '；').strip()


def objects(ext):
    data = json.loads((RAW / f'war3map.{ext}.json').read_text(encoding='utf-8'))
    out = {}
    for group in ('original', 'custom'):
        for index, obj in enumerate(data[group]):
            oid = obj['new_id'].strip('\0') or obj['base_id']
            fields = {}
            for mod in obj['modifications']:
                fields.setdefault(mod['field'], {})[str(mod.get('level', 0))] = mod['value']
            out[oid] = {'base_id': obj['base_id'], 'source': f'war3map.{ext}.json', 'pointer': f'/{group}/{index}', 'fields': fields}
    return out


ABILITIES, TECHS, UNITS, ITEMS = (objects(ext) for ext in ('w3a', 'w3q', 'w3u', 'w3t'))


def field(table, oid, key, level=0, default=None):
    return table[oid]['fields'].get(key, {}).get(str(level), default)


# Name, slot, learn tech, upgrade tech, ability, normal cap, active functions.
SKILLS = [
    ('闪电', 'D', 'R009', 'R00P', 'S002', 7, ['tc', 'kc']),
    ('回旋镖', 'D', 'R00Z', 'R010', 'S004', 7, ['nC', 'oC']),
    ('追踪球', 'D', 'R005', 'R00O', 'S003', 7, ['Zc', 'uc', 'wc']),
    ('瞬间移动', 'R', 'R004', 'R00V', 'S011', 7, ['Xd', 'Bd', 'bd']),
    ('疾速移动', 'R', 'R007', 'R00W', 'S012', 7, ['PC', 'qd', 'Mb']),
    ('移形换位', 'R', 'R013', 'R014', 'S013', 6, ['Dd', 'Fd', 'Cd']),
    ('汲取', 'T', 'R00E', 'R00L', 'S014', 6, ['yd', 'Td', 'Ud']),
    ('火焰喷射', 'T', 'R001', 'R00J', 'S015', 6, ['iD', 'vD', 'Yd', 'zd']),
    ('弹跳球', 'T', 'R003', 'R00K', 'S016', 6, ['bD', 'AD', 'OD', 'XD']),
    ('流星', 'E', 'R00A', 'R00R', 'S008', 6, ['dC', 'NC', 'mC', 'kC', 'KC']),
    ('疾风步', 'E', 'R00F', 'R00X', 'S010', 6, ['MC', 'pC', 'Mb', 'mb']),
    ('分裂弹', 'E', 'R00H', 'R00U', 'S009', 6, ['zC', 'sC', 'xd']),
    ('爆炎', 'E', 'R018', 'R01D', 'S030', 6, ['nd', 'rd', 'xd']),
    ('急行', 'C', 'R006', 'R00S', 'S007', 6, ['PA', 'mA', 'pA']),
    ('反射盾', 'C', 'R00C', 'R00T', 'S005', 6, ['Gf', 'Ff', 'ff']),
    ('时光回溯', 'C', 'R011', 'R012', 'S006', 6, ['Sf', 'qf', 'Pf']),
    ('镜像分身', 'C', 'R01B', 'R01C', 'S048', 6, ['pf', 'kf', 'Jf']),
    ('锁链', 'Y', 'R00B', 'R00Q', 'S019', 7, ['tD', 'TD', 'ub']),
    ('禁锢', 'Y', 'R008', 'R00M', 'S017', 6, ['xf', 'wD', 'uD', 'df']),
    ('引力', 'Y', 'R00G', 'R00N', 'S018', 5, ['DD', 'BD', 'HD', 'GD', 'gD']),
    ('操纵', 'Y', 'R019', 'R01A', 'S050', 6, ['Vf', 'rf', 'Xf', 'UB']),
]

NOTES = {
    '闪电': '脚本基础伤害为 6+L，击退系数 0.92；基础射程 600×(1+0.12×远程精通)。闪电与火球相遇有专门引爆分支，不应简化成只命中角色的射线。',
    '回旋镖': '首击基础伤害 6.3+0.7L；第 h 次既往命中后，下一次伤害乘 1+0.1h，击退系数为 0.95/(1+0.1h)。与商店所写每级增加 0.6 不一致。向外与返程是同一实体，不能将返程做成新发射。',
    '追踪球': '基础伤害 6+L，寿命 4.5×(1+0.12×远程精通) 秒。飞行超过前 2.25 秒后还存在随剩余寿命计算的增伤；不是恒定 7～13 伤害。施法者接触可触发范围爆炸，精确分支见 uc/wc。',
    '瞬间移动': '闪现距离 700+70L。闪现先去掉当前主动步行分量，再按 Xd 分段保留或削减外力，不能简单清零速度。折跃第一段 300+50L，3.3 秒窗口内第二段最多 300；折跃按脚本调用 Rd，不直接套用闪现的速度削减。',
    '疾速移动': '冲撞基础满速伤害 5+0.4L；实际伤害还与当前速度平方有关，Mb 分支在速度不足时按比例降低。驭凤持续 2.7×时间倍率秒，禁锢状态下为 1×时间倍率秒；质量参数减少 0.6。施法冲量、碰撞取消与隐身联动需保留。',
    '移形换位': '移形与首次遇到的术士或柱子换位，否则抵达标记；友军处于安全地面而自己在岩浆时存在额外限制。换位模式交换飞行法术并附加移速。重叠坐标和无合法目标时必须定义安全失败分支。',
    '汲取': '汲取模式实际首击 5+L，击退系数 0.2，速度变化 50，持续 (3+L)×施法者时间倍率×目标负面时间倍率。虚弱首击 4+L，击退系数 0.6，敌人输出乘 0.5，持续 7.5×双方相关时间倍率；命中友军则输出乘 1.1、回血并继续寻敌。返回施法者的治疗球还会清除半额击退值。',
    '火焰喷射': '连珠每枚 2.3+0.3L 伤害，击退系数 0.6；共 5+L 枚，相邻发射间隔 0.08 秒。齐射每枚 3.3+0.3L，击退系数 0.65；iD 循环生成 4 枚。商店的齐射公式按升级次数解释时低估一级伤害。',
    '弹跳球': '弹射模式初始伤害 4.6+0.8L，命中后伤害倍率乘 0.7，最低 0.2；并非说明所写每次减少 20% 直至没有伤害。常规击退系数 1.2，特定早期低倍率分支为 0.8。惩罚模式基础伤害 (5.3+0.7L)×(1+0.05×连续命中计数)，击退系数 1.15；命中可复位 T 冷却。',
    '流星': '陨石脚本 NC 按中心距离平方衰减，使用 Lv=10+L，伤害候选为 Lv−攻击者平减−(中心距离/85)^2，超过门槛才命中；视觉半径与实际命中判定不可混为一谈。滚石是可变半径实体，会因命中飞弹而增长，有接触减速、周期伤害和结束爆炸；其数值见 kC/KC，不应用商店一行伤害替代整个过程。',
    '疾风步': '影袭持续 2.6×时间倍率秒、移速加 200；潜行持续 4×时间倍率秒、移速加 200/3。影袭和冲撞有专门组合分支：Mb 中各自伤害会合并，不能作为两个独立命中机械相加。潜行的碰撞伤害与生命偷取见 mb。',
    '分裂弹': '爆裂母弹可瞄准的基础距离 700×远程倍率；小弹 sC 首击 2.5+0.5L，击退系数 0.65。散射子弹 xd 为 2.7+0.3L，击退系数 0.6。母弹、柱子反弹与小弹分布是独立步骤。',
    '爆炎': '实际购买上限为 6 级，商店写 7 级。发射器距施法者最多 100，持续 2.8×时间倍率秒；周期生成子弹，子弹伤害 2.7+0.3L，击退系数 0.6。rd 在 Le 时发射，Le 为每 6 个 0.03 秒步触发一次，即约 0.18 秒。',
    '急行': '脚本持续 (5.5+0.5L)×时间倍率秒，初始移速加 35；可吸收伤害池 3+2L，每次最多吸收该次基础伤害的一半，吸收 1 点转换为 15 移速。吸收发生在最终伤害倍率前；原脚本击退却先计算，是否有意保留击退需运行与机制核对，未确认前保留原行为，见缺陷文档。',
    '反射盾': '持续 (2.6+0.2L)×时间倍率秒；创建半径 90+角色半径的反射体，能反射特定飞弹和冲撞，不提供通用范围伤害无敌。',
    '时光回溯': '施放时建立快照，3.5×时间倍率秒后返回该快照，也可提前触发。HP 只在当前低于快照时恢复；击退值只对期间新增且为正的部分保留 (0.8-0.1L)，即一级 70%、六级 20%。位置、动量、朝向和冲撞状态也恢复；不是读取施放前 3.5 秒的历史。',
    '镜像分身': '先有最长 4 秒的点选触发阶段，触发后形成两个非实体镜像，持续 4×时间倍率秒、移速加 25。普通镜像火球 Ac/Ic 伤害为 0.5L，击退系数为 1/(0.5L)，因此不能照搬普通火球的 Q=1。镜像其他特殊弹道联动需逐个核对。',
    '锁链': '脚本普通学习后可升级 6 次，因此基础上限为 7；说明只列出 6。蓝链拉对方、红链拉自己；连接伤害和牵引每物理步更新，伤害在 Le 步触发。牵引与双方质量、断链、二次施法、柱子锚定共同决定，不能用一次性击退替代。',
    '禁锢': '缠绕持续 (4+0.25L)×施法者时间倍率×目标负面时间倍率；一级为 4.25 秒，商店按升级次数读得 4.5 秒存在偏差。期间基础移速归零、受击退倍率除 3、施法自损参数增加 5；解除时恢复。沉默具有按技能槽分别封锁的时间，F 不受影响。',
    '引力': '黑洞飞弹速度 400、基础最大路程 900×远程倍率；牵引在 0.03 秒步更新、近距离伤害在约 0.18 秒步触发，每次 0.1+0.2L。暗域是半径 200、持续 5×时间倍率秒的地面场；友军每 0.18 秒回复最大生命×(0.0012+0.0003L)，敌军持续伤害与减速按 gD/fD。',
    '操纵': '扰乱命中后使目标生命偷取负抗性参数加 0.3、持续倍率减 0.3，并改写法术瞄准源。点阵允许从标记施法，标记最长 120 秒；增强下一次施法的精通表由 UB 定义，持续 2×临时增强后的时间倍率秒，不是整个标记期间永久增强。',
}


def fmt(value):
    if isinstance(value, float):
        return f'{value:.6g}'
    return str(value)


def main():
    records = []
    text = ['# 术士之战技能和购买规则', '', '本章以 1.00b 汉化版的物体字段和 JASS 为基准。L 表示技能实际等级，从 1 开始；“升级次数”为 L−1。数值单位为原地图单位，未换算成 UE 厘米。', '',
            '## 技能槽与购买限制', '', '开局自带 G 火球和角色 F 技能。六个可购买技能槽各选一种，不能在同槽同时学习两种。模式切换不收费、不额外占槽。技能升级不会占用新槽。', '',
            '学习第 1～3 个新技能使用下表原价；第 4、5、6 个分别在原价上增加 1、2、3 金。该加价由已学习槽数决定，独立于技能升级价。依据：'+source('vj')+'、'+source('ZH')+'。', '',
            '| 槽 | 技能 | 首次学习原价 | 升至 2 级起的逐级价格 | 普通上限 | 购买 ID | 升级 ID |', '|---|---|---:|---|---:|---|---|']
    for name, slot, learn, upgrade, ability, cap, fns in SKILLS:
        base = field(TECHS, learn, 'gglb')
        step = field(TECHS, upgrade, 'gglb')
        prices = [step + n for n in range(cap - 1)]
        text.append(f'| {slot} | {name} | {base} | {" / ".join(map(str, prices))} | {cap} | `{learn}` | `{upgrade}` |')
        record = {'name': name, 'slot': slot, 'learn_id': learn, 'upgrade_id': upgrade, 'ability_id': ability, 'learn_gold_base': base, 'upgrade_gold_base': step, 'upgrade_gold_increment': field(TECHS, upgrade, 'gglm', default=0), 'normal_level_cap': cap, 'script_functions': {fn: FUNCTIONS[fn] for fn in fns}, 'learn_object': TECHS[learn], 'upgrade_object': TECHS[upgrade], 'ability_object': ABILITIES[ability], 'script_notes': NOTES[name]}
        records.append(record)
    text += ['', '研究价按 `基础价 + 已完成该项研究次数 × 每级增量` 计算；各技能升级增量字段为 1。乔丹之石允许选定技能槽再升 2 级，额外等级继续沿用升级价递增，不是免费获得 2 级。火球普通上限 10，石头后 12；首次火球升级价 3，此后每次加 1，且受任意精通等级门槛约束。', '',
             '## 火球和天罚', '', 'G 火球：一级 7 伤害，每级增加 0.5，冷却 4.8 秒；速度 1000/秒，碰撞半径 25，基础寿命 1 秒，远程精通延长寿命。普通直击的击退系数为 1。燃烧权杖会替换直接伤害公式及追加燃烧，不属于默认物品配置。依据：'+source('jc')+'、'+source('hc')+'。', '',
             'F 天罚为角色特有的近身技能，不能将四个角色统一为一个伤害模板。普通术士的 F 冷却 3 秒、自损 10 HP（不自杀，最低保留 0.5）、敌方基础伤害 10、半径 250；击退系数为 1−距离/1000。血剑增加伤害和命中后的击退值清除。依据：'+source('tf')+'。其余角色及装备变体的物体字段和函数见机器表；第一版先使用术士，扩展角色须通过专项验收。', '',
             '## 各技能字段和脚本行为', '', '冷却来自技能 `acdn` 字段，而非商店描述。显示“缺省”表示当前地图没有覆盖该层字段，不可据此填 0。多数双模式以技能物体第 1～10 层和第 11～20 层保存；实际层映射须与 BF 的切换逻辑一起使用。下表列出基础上限和乔丹之石后的两级；对单模式不解释虚构的第二模式。']
    dual = {'S008', 'S009', 'S010', 'S011', 'S012', 'S013', 'S014', 'S015', 'S016', 'S017', 'S018', 'S019', 'S050'}
    for rec in records:
        name, ability, cap = rec['name'], rec['ability_id'], rec['normal_level_cap']
        text += ['', f'### {name}', '', f"快捷键 {rec['slot']}，技能物体 `{ability}`。", '', NOTES[name], '', '脚本依据：'+'、'.join(source(fn) for fn in rec['script_functions'])+'。', '', '| 等级 | 模式一冷却秒 | '+('模式二冷却秒' if ability in dual else '备注')+' |', '|---:|---:|---|']
        for level in range(1, cap + 3):
            a = field(ABILITIES, ability, 'acdn', level, '缺省')
            b = field(ABILITIES, ability, 'acdn', level + 10, '缺省') if ability in dual else ('乔丹之石扩展等级' if level > cap else '')
            text.append(f'| {level} | {fmt(a)} | {fmt(b)} |')
        requirements = TECHS[rec['upgrade_id']]['fields']
        gates = []
        for k, value in requirements.get('greq', {}).items():
            if value:
                gates.append(f"研究第 {k} 次（技能升至 {int(k)+1} 级）：`{value}` × {requirements.get('grqc', {}).get(k, '原生缺省，待核对')}")
        if gates:
            text += ['', '物体表中的升级前置条件：', ''] + ['- '+g for g in gates]
        tips = []
        for val in TECHS[rec['learn_id']]['fields'].get('gub1', {}).values():
            val = clean(val)
            if val and val not in tips:
                tips.append(val)
        if tips:
            text += ['', '原商店说明仅作为对照，遇到冲突采用上面的字段与脚本：', '', '> '+tips[0].replace('|','\\|')]
    text += ['', '## 默认可选和保留条目', '', '主目录共识别 21 个可购买技能族。`R015/R016` 为“未命名”遗留条目，不计入正式技能目录。镜像分身、操纵、爆炎位于第二页辅助商店；地图保留按配置禁用或随机删除技能的能力。配置应保存开放名单，不可仅凭文件存在就使所有遗留条目可购买。', '',
             '说明中“追踪球要范围精通”的文字已经过时；此版移除了范围精通，实际部分门槛改用 `R017` 任意精通。所有门槛的原始字段已保留在机器表，缺省值必须通过原游戏或对应引擎默认数据确认。']
    (DOCS / '02_SkillsAndShop.md').write_text('\n'.join(text)+'\n', encoding='utf-8')
    manifest = {'source_map': 'Warlock1.00b CN.w3x', 'source_sha256': hashlib.sha256((ROOT/'Warlock1.00b CN.w3x').read_bytes()).hexdigest(), 'functions': FUNCTIONS, 'skills': records, 'abilities': ABILITIES, 'technologies': TECHS, 'units': UNITS, 'items': ITEMS}
    (RAW / 'reference_data.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    # Preserve every native data field; this is an evidence export, not ready-to-run UE configuration.
    item_lines = ['# 术士之战物品精通和外观目录', '', '购买价格采用商店训练单位的 `ugol`，不是背包物品的 `igol`。售出采用 Ah 中显式返还金币。升级按重复购买同一商店条目进行；背包物品会被替换为下一等级，不额外占一个槽。', '', '## 战斗物品价格', '', '| 物品 | 商店 ID | 每次购买金 | 最高等级 | 各等级售出返还金 | 默认所在页 |', '|---|---|---:|---:|---|---|']
    shop = [('h00E',1,'3',True),('h00C',3,'5 / 10 / 15',True),('h007',2,'7 / 14',True),('h004',1,'不可出售',True),('h00B',3,'4 / 8 / 12',True),('h002',3,'2 / 4 / 6',True),('h00A',3,'3 / 6 / 9',True),('h005',3,'5 / 10 / 15',True),('h006',1,'3',False),('h008',1,'9',False),('h00G',2,'2 / 2',False),('h00F',1,'2',False),('h001',1,'6',False)]
    for oid, cap, refund, enabled in shop:
        name = clean(field(UNITS, oid, 'unam'))
        item_lines.append(f'| {name} | `{oid}` | {field(UNITS,oid,"ugol")} | {cap} | {refund} | {"第一页" if enabled else "第二页"} |')
    item_lines += ['', '依据：'+source('Ih')+' 初始化 ME 前 8 项为第一页；'+source('gF')+' 翻页时开放下一组条目，因此第二页不是默认禁用内容；'+source('Jh')+' 处理购买与互斥；'+source('Ah')+' 处理出售。', '',
                   '## 脚本确认的装备效果', '',
                   '| 商店物品 | 实际效果或持久参数 |', '|---|---|',
                   '| 速度之靴 | 移速总加20/30/40；质量参数总减0.05；与熔岩靴互斥 |',
                   '| 血牌 | 最大HP增加10/20/30，生命偷取负抗性总加0.05/0.10/0.15；三级自然回血每tick再加0.01，即每秒0.1 |',
                   '| 恢复之戒 | 自然回血每tick总加0.02/0.035/0.045，即每秒0.2/0.35/0.45；生命偷取负抗性总加0.06/0.10/0.10 |',
                   '| 头盔 | 受击退倍率分别乘0.84/0.76/0.68；质量参数总加0.1/0.2/0.3；有效远程精通总减1/2/3（普通射程倍率因此减12/24/36个百分点） |',
                   '| 熔岩靴 | 移速总加13/24/35，生命偷取负抗性加0.05；Xc使岩浆伤害乘1/8，持续(3/4/5)×时间倍率秒，28秒冷却；HP<10.5补到10.5 |',
                   '| 抗魔斗篷 | 目标负面持续倍率乘0.65；自身时间倍率减0.05，是减5个百分点，不是乘0.95 |',
                   '| 诅咒徽章 | 最大HP加10；受击退倍率乘0.85；负面持续倍率乘1.25 |',
                   '| 守护指环 | 直接伤害平减总加0.5/1；原伤害路径同时扣攻击者平减，意图待确认，见BUG01 |',
                   '| 吸血药水 | 常驻生命偷取负抗性减0.2；F施法时临时生命精通加5，结束还原；角色F专门分支另有击退值恢复 |',
                   '| 燃烧权杖 | 火球改为基础直伤6.1+0.4L，叠加燃烧计数；每次有效火球命中增加计数，丢失火球时重置；燃烧持续和倍率见hc/Cb |',
                   '| 鲜血之剑 | 两级，提高角色F等级和Xi；普通术士F敌伤11/12，每命中者清除自身K 2/3；其他角色F需各自核对 |',
                   '| 守护之盾 | 最大HP减10、质量加0.2；nc激活效果使受伤倍率除4/3（即乘0.75）、受击退除2，持续5×时间倍率秒；触发与F状态bn相关 |',
                   '| 乔丹之石 | 1金、一次性选择一个槽扩展两级上限；不直接升技能；不可售出 |', '',
                   '商店名与背包名在汉化版不完全一致：恢复之戒对应背包“生命披风”，抗魔斗篷对应“恶魔怀表”，吸血药水对应“死亡面罩”，燃烧权杖对应“火球权杖”，守护指环对应“守护臂章”。UE 使用统一显示名并保留这些别名用于查证，不按名称字符串连接商品。', '',
                   '头盔减少的是有效远程精通，不减少已购买远程研究次数；出售时应恢复有效等级。原脚本售价与返还路径覆盖了该还原。装备与技能产生的临时数值要通过独立实例管理。', '',
                   '## 原商店描述', '', '下面保留商店描述供检索；描述与脚本冲突按缺陷文档处理。完整实际字段见 reference_data.json 的 units/items。']
    for oid, _, _, _ in shop:
        item_lines += ['', '### '+clean(field(UNITS,oid,'unam')), '', clean(field(UNITS,oid,'utub',default='无描述'))]
    item_lines += ['', '## 精通和背包', '', '| 项目 | 研究 ID | 价格规则 | 上限与效果 |', '|---|---|---|---|',
                   '| 生命精通 | R00D | 6 / 7 / 8 / 9 / 10 / 11 | 6 级；每级伤害吸血 10%×目标生命偷取负抗性 |',
                   '| 远程精通 | R00Y | 5 / 6 / 7 / 8 / 9 / 10 | 6 级；常见射程或寿命加 12%，具体技能有例外；头盔使有效远程精通每级减 1 |',
                   '| 时间精通 | R00I | 4 / 5 / 6 / 7 / 8 / 9 | 6 级；持续时间倍率每级加 0.08，不等于减少所有冷却 |',
                   '| 博而不专 | R01E | 13 + 当前三类研究次数总和 | 三种精通各升 1；相比分别购买省 2 金；任意一类到顶则禁止继续套餐 |',
                   '| 背包 | R000 | 当前表覆盖基础价 2，未覆盖增量 | 开局已有研究 1，S128 默认第 1 级提供 2 格；后续到 4、6 格。下一档价格需原生升级默认字段确认 |', '',
                   '每获得一个永久精通等级，基础受击退系数线性减少 0.025，基础回复倍率增加 0.05；三个精通各一级即减少 7.5% 受击退、增加 15% 回复倍率。精通减击退按总等级线性计算，不能按每级连续相乘 0.975；与头盔、状态倍率分开。依据：'+source('vj')+'。', '',
                   '## 物品互斥和出售', '', '速度之靴与熔岩靴互斥；鲜血之剑与守护之盾互斥。其他物品的同名升级上限、可叠加与背包判定逐项采用 Jh。血牌最高三级后仍有重新购买低级血牌的代码分支，不能擅自全局禁止同族叠加，需按缺陷文档验证。乔丹之石只能购买一次并对一个技能槽启用，启用后扩展两级上限，石头不可出售。', '',
                   '生命恢复之戒原说明售出 3/6/9 或每级 2/3 金互相矛盾，Ah 实际为 2/4/6；守护指环二级售出也只有 2。开发必须显示实际返还金额。', '',
                   '## 外观购买目录', '', '令符是独立于战斗金币的资源，下表保留地图实际商店价格。外观通过物品商店另一组页面及令符系统购买。开发版先保留数据，不复制原地图按玩家名字绑定权限或预置余额的系统。', '', '| ID | 名称 | 金币 | 令符 | 原说明 |', '|---|---|---:|---:|---|']
    for oid, obj in UNITS.items():
        if (oid.startswith('i') and len(oid)==4 and oid[1:].isdigit()) or (oid.startswith('j') and oid[1:].isdigit()):
            item_lines.append(f'| `{oid}` | {clean(field(UNITS,oid,"unam"))} | {field(UNITS,oid,"ugol",default="缺省")} | {field(UNITS,oid,"ulum",default="缺省")} | {clean(field(UNITS,oid,"utub",default="")).replace("|","/")} |')
    (DOCS / '03_ItemsAndMasteries.md').write_text('\n'.join(item_lines)+'\n', encoding='utf-8')
    print(f'Generated {len(records)} skill families, {len(shop)} item families; map SHA-256 {manifest["source_sha256"]}')


if __name__ == '__main__':
    main()
