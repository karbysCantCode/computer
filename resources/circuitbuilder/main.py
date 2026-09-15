
import sys
import shlex
import shutil

from parser import parse
from graph import Graph
from netlist import render_netlist
from logisim import build_circ
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent


def resolve_path(path):
  path = Path(path)

  if path.is_absolute():
    return path

  return BASE_DIR / path


def compile_file(src_path, netlist_out, circ_out):
  src_path = resolve_path(src_path)
  netlist_out = resolve_path(netlist_out)
  circ_out = resolve_path(circ_out)

  with open(src_path) as f:
    text = f.read()

  stmts = parse(text)

  g = Graph()
  g.run_program(stmts)

  nl = render_netlist(g)

  with open(netlist_out, "w") as f:
    f.write(nl)

  # Back up the existing circuit before overwriting it.
  if circ_out.exists():
    backup_path = circ_out.with_name(
      f"{circ_out.stem}_netlistbackup{circ_out.suffix}"
    )

    shutil.copy2(circ_out, backup_path)

  circ = build_circ(g, circ_out, netlist_out)

  with open(circ_out, "w") as f:
    f.write(circ)

  return g, nl


def print_help():
  print("""
Commands:

  compile <source> <netlist> <circuit>
      Compile a source file.

  help
      Show this help message.

  exit
      Exit the compiler.

  quit
      Exit the compiler.

Examples:

  compile program.txt program.netlist program.circ

  compile "program.txt" "program.netlist" "program.circ"
""")


def main():
  print("ELCS Compiler")
  print("Type 'help' for help.")
  print()

  while True:
    try:
      command = input("> ").strip()
    except (EOFError, KeyboardInterrupt):
      print()
      break

    if not command:
      continue

    try:
      parts = shlex.split(command, posix=False)
    except ValueError as e:
      print(f"Error: {e}")
      continue

    # Remove surrounding quotes from arguments.
    parts = [
      part[1:-1]
      if len(part) >= 2 and part[0] == part[-1] and part[0] in "\"'"
      else part
      for part in parts
    ]

    cmd = parts[0].lower()

    if cmd in ("exit", "quit"):
      break

    elif cmd == "help":
      print_help()

    elif cmd == "compile":
      if len(parts) != 4:
        print("Usage: compile <source> <netlist> <circuit>")
        continue

      src_path = parts[1]
      netlist_out = parts[2]
      circ_out = parts[3]

      try:
        g, nl = compile_file(
          src_path,
          netlist_out,
          circ_out
        )

        print()
        print(nl)
        print("Compiled successfully.")
        print(f"Netlist: {netlist_out}")
        print(f"Circuit: {circ_out}")

      except FileNotFoundError as e:
        print(f"Error: file not found: {e.filename}")

      except Exception as e:
        print(f"Error: {e}")

    else:
      print(f"Unknown command: '{cmd}'")
      print("Type 'help' for available commands.")


if __name__ == "__main__":
  main()
