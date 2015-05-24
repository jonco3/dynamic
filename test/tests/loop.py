# output: ok

count = 0
total = 0
last = 0
for i in (1, 2, 3):
    count += 1
    total += i
    last = i
assert count == 3
assert total == 6
assert last == 3

count = 0
total = 0
last = 0
i = 1
while i <= 3:
    count += 1
    total += i
    last = i
    i += 1
assert count == 3
assert total == 6
assert last == 3

count = 0
total = 0
last = 0
for i in (1, 2, 3):
    count += 1
    total += i
    last = i
    if i == 2:
        break
assert count == 2
assert total == 3
assert last == 2

count = 0
total = 0
last = 0
i = 1
while i <= 3:
    count += 1
    total += i
    last = i
    if i == 2:
        break
    i += 1
assert count == 2
assert total == 3
assert last == 2

count = 0
total = 0
last = 0
for i in (1, 2, 3):
    if i == 2:
        continue
    count += 1
    total += i
    last = i
assert count == 2
assert total == 4
assert last == 3

count = 0
total = 0
last = 0
i = 1
while i <= 3:
    if i == 2:
        i += 1
        continue
    count += 1
    total += i
    last = i
    i += 1
assert count == 2
assert total == 4
assert last == 3

f = 0
for i in (1, 2, 3):
    try:
        if i == 2:
            break
    finally:
        f = i
assert f == 2

f = 0
for i in (1, 2, 3):
    try:
        try:
            try:
                if i == 1:
                    break
            finally:
                f += 1
        except Exception:
            pass
    finally:
        f += 1
assert f == 2

f = 0
for i in (1, 2, 3):
    try:
        if i == 2:
            continue
    finally:
        f += 1
assert f == 3

print('ok')
