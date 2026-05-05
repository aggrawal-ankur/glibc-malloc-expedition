base_offset = 8
bin_hdrs = 0

for bin_num in range(1, 127):
  print(f'Bin #{bin_num} -> {bin_hdrs, bin_hdrs+1}: <main_arena+{base_offset}>')
  bin_hdrs += 2
  base_offset += 16
