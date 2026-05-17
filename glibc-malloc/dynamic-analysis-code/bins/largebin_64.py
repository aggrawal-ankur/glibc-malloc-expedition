SMALLBIN_WIDTH = 16
CAT1_BASE = 1024

print("Category: #1, Total bins: 32, Width: 64 bytes, Fixed Classes: 4\n")
print("| Sr. | Bin # | Base Size | Fixed Classes |")
print("| --- | ----- | --------- | ------------- |")

for i in range(64, 64+32):
  print(f"| {i-63} | {i} | {CAT1_BASE} | ", end='')

  for j in range(4):
    print(CAT1_BASE, end=', ')
    CAT1_BASE += SMALLBIN_WIDTH

  print("|")

print()
print()


CAT2_BASE = CAT1_BASE

print("Category: #2, Total bins: 16, Width: 512 bytes, Fixed Classes: 32\n")
print("| Sr. | Bin # | Base Size | Fixed Classes |")
print("| --- | ----- | --------- | ------------- |")

for i in range(96, 96+16):
  print(f"| {i-95} | {i} | {CAT2_BASE} | ", end='')

  for j in range(32):
    print(CAT2_BASE, end=', ')
    CAT2_BASE += SMALLBIN_WIDTH

  print("|")

print()
print()


CAT3_BASE = CAT2_BASE

print("Category: #3, Total bins: 8, Width: 4096 bytes, Fixed Classes: 256\n")
print("| Sr. | Bin # | Base Size | Fixed Classes |")
print("| --- | ----- | --------- | ------------- |")

for i in range(112, 112+8):
  print(f"| {i-111} | {i} | {CAT3_BASE} | ", end='')

  for j in range(256):
    print(CAT3_BASE, end=', ')
    CAT3_BASE += SMALLBIN_WIDTH

  print("|")

print()
print()


CAT4_BASE = CAT3_BASE

print("Category: #4, Total bins: 4, Width: 32768 bytes, Fixed Classes: 2048\n")
print("| Sr. | Bin # | Base Size | Fixed Classes |")
print("| --- | ----- | --------- | ------------- |")
for i in range(120, 120+4):
  print(f"| {i-119} | {i} | {CAT4_BASE} | ", end='')

  for j in range(2048):
    print(CAT4_BASE, end=', ')
    CAT4_BASE += SMALLBIN_WIDTH

  print("|")

print()
print()


CAT5_BASE = CAT4_BASE

print("Category: #5, Total bins: 2, Width: 262144 bytes, Fixed Classes: 16384\n")
print("| Sr. | Bin # | Base Size | Fixed Classes |")
print("| --- | ----- | --------- | ------------- |")
for i in range(124, 124+2):
  print(f"| {i-123} | {i} | {CAT5_BASE} | ", end='')

  for j in range(16384):
    print(CAT5_BASE, end=', ')
    CAT5_BASE += SMALLBIN_WIDTH

  print("|")

print()
print()


CAT6_BASE = CAT5_BASE

print("Category: #6, Total bins: 1, Width: 2097152 bytes, Fixed Classes: 131072\n")
print("| Sr. | Bin # | Base Size | Fixed Classes |")
print("| --- | ----- | --------- | ------------- |")
for i in range(126, 126+1):
  print(f"| {i-125} | {i} | {CAT6_BASE} | ", end='')

  for j in range(131072):
    print(CAT6_BASE, end=', ')
    CAT6_BASE += SMALLBIN_WIDTH

  print("|")
