def main_logic():
  base_size = 20
  add = 16  # Take anything.
  size_class = [i*16 for i in range(2, 63+2)]

  print('| Size | [target_c, next_c] | request2size(Size) ')
  print('| ---- | ------------------ | ------------------ ')
  print(f'| 20 | [{size_class[0]}, {size_class[1]}] | {(20 + 23) & ~15} |')

  i = 1
  while (i<62):
    base_size += add
    print(f'| {base_size} | [{size_class[i]}, {size_class[i+1]}] | {(base_size + 23) & ~15} |')
    i += 1


def pretty_print():
  import os

  # Create the output directory
  out_dir = 'add_val_output'
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)

  # Generate the size classes
  size_class = [i*16 for i in range(2, 63+2)]

  # Generate the outcome for multiple add values: [10, 19]
  for i in range(10):
    base_size = 20
    add = 10 + i    # New add value for each iteration

    path = f"./add_val_output/add_{add}"    # File path for an iteration
    with open(path, 'w') as f:
      f.write(f"Base = {base_size}\nAdd = {add}\n\n")
      f.write('| Size | [target, next] | request2size(Size) | Matched? |\n')
      f.write('| ---- | -------------- | ------------------ | -------- |\n')
      _inter = f'[{size_class[0]}, {size_class[1]}]'
      _res = (20 + 23) & ~15
      f.write(f'| {20:<4} | {_inter:<14} | {_res:<18} | {'1':<8} |\n')

      j = 1
      while (j<62):
        base_size += add
        interval = f'[{size_class[j]}, {size_class[j+1]}]'
        result = (base_size + 23) & ~15
        status = '1' if (result == size_class[j]) else '0'
        f.write(f'| {base_size:<4} | {interval:<14} | {result:<18} | {status:<8} |\n')
        j += 1


pretty_print()
