# -*- coding: utf-8 -*-
"""
Проверка ответов по генератору неисправностей.
Формат ответа обучающегося (JSON): {"case_01.csv": {"fault": "turn_fault", "phase": "B"}, ...}
Запуск: python check_answers.py answers_student.json cases/answers_instructor.json
Вид неисправности — 1 балл, фаза (если применимо) — 0,5 балла. Обоснование проверяет преподаватель.
"""
import sys, json
stu = json.load(open(sys.argv[1], encoding="utf-8"))
ref = json.load(open(sys.argv[2], encoding="utf-8"))
score = total = 0.0
for case, r in ref.items():
    s = stu.get(case, {})
    total += 1 + (0.5 if r["phase"] else 0)
    ok_f = s.get("fault") == r["fault"]
    ok_p = r["phase"] is None or (ok_f and s.get("phase") == r["phase"])
    score += ok_f + (0.5 if (r["phase"] and ok_p) else 0)
    print(f"{case}: {'верно' if ok_f else 'неверно'}" + ("" if r["phase"] is None else f", фаза {'верно' if ok_p else 'неверно'}"))
print(f"Итого: {score:.1f} из {total:.1f}")
