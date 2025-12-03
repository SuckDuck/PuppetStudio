import os, sys
from typing import List

class COLORS:
    RED    = '\033[31m'
    GREEN  = '\033[32m'
    YELLOW = '\033[33m'
    BLUE   = '\033[34m'
    RESET  = '\033[0m'

def have_changed(in_files:List[str], out_file:str) -> bool:
    """Return True if out_file is missing or older than any file in in_files."""
    
    if(not os.path.exists(out_file)): return True
    out_path_date = os.path.getmtime(out_file)

    for f in in_files:
        if not os.path.exists(f): return True
        if os.path.getmtime(f) > out_path_date: return True
    return False

def create_dirs(routes:List[str]):
    """Create directories if they don't already exist."""

    for r in routes:
        if(not os.path.exists(r)):
            os.makedirs(r)

def rm_all(path:str, excludes:List[str] = []):
    """Remove all files in the given directory (does not remove subdirectories)."""

    for f in os.listdir(path): 
        if f in excludes: continue
        os.remove(os.path.join(path,f))

def create_statics(static_dir:str, assets:List[str]):
    """Convert each asset file into a C header and save it in static_dir.
    Skips files that haven't changed since the last conversion."""

    for a in assets:
        out = os.path.join(static_dir,a.split(os.sep)[-1].split(".")[0]+".h")
        if(not have_changed([a],out)):
            print(f"{out} {COLORS.BLUE}-> nothing to do!{COLORS.RESET}")
            continue

        og_var_name = a.replace(" ","_").replace(os.sep,"_").replace(".","_").replace("-","_")
        new_var_name = a.split(os.sep)[-1].replace(" ","_").replace(".","_").replace("-","_")

        command = f"xxd -i {a} | sed 's/{og_var_name}/{new_var_name}/g' > {out}"
        print(f"{COLORS.GREEN}COMMAND: {COLORS.RESET} {command}")
        if(os.system(command) != 0): sys.exit(1)
        
def compile(cc:str, cflags:List[str], macros:dict, includes:List[str], sources:List[str], out_dir:str) -> List[str]:
    """ Compile C source files into object files, skipping unchanged files.
    Returns the list of created object files. """
    
    cflags = " ".join(cflags)
    includes = " ".join([f"-I{i}" for i in includes])
    macros = " ".join([f"-D{i[0]}" + (f"={i[1]}" if i[1] != "" else "") for i in macros.items()])
    
    out_files = []
    for s in sources:
        out = os.path.join(out_dir,s.split(os.sep)[-1].replace(".c",".o"))
        out_files.append(out)
        if(not have_changed(s.split(" "),out)):
            print(f"{out} {COLORS.BLUE}-> nothing to do!{COLORS.RESET}")
            continue

        command = f"{cc} {cflags} {macros} -c -o {out} {includes} {s}"
        print(f"{COLORS.GREEN}COMMAND: {COLORS.RESET} {command}")
        if(os.system(command) != 0): sys.exit(1)

    return out_files

def link(cc:str, sources:List[str], lib_paths:List[str], links:List[str], ldflags:List[str], out_dir:str, out_name:str):
    """Link object files into an executable in out_dir, skipping if up-to-date."""
    
    links=" ".join(links)
    lib_paths=" ".join(lib_paths)
    ldflags=" ".join(ldflags)

    out_name = os.path.join(out_dir,out_name)
    if(not have_changed(sources,out_name)):
        print(f"{out_name} {COLORS.BLUE}-> nothing to do!{COLORS.RESET}")
        return
    
    sources = " ".join(sources)
    command = f"{cc} -o {out_name} {sources} {lib_paths} {links} {ldflags}"
    print(f"{COLORS.GREEN}COMMAND: {COLORS.RESET} {command}")
    if(os.system(command) != 0): sys.exit(1)

if __name__ == "__main__":
    print(f"{COLORS.YELLOW}This file is not meant to be executed directly.{COLORS.RESET}")
    print(f"{COLORS.YELLOW}Please use build.py{COLORS.RESET}")