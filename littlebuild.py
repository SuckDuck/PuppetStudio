import os, sys, zipfile
from typing import List, Union, Dict, Optional
from pathlib import Path
import subprocess

class COLORS:
    RED    = '\033[31m'
    GREEN  = '\033[32m'
    YELLOW = '\033[33m'
    BLUE   = '\033[34m'
    RESET  = '\033[0m'

def __command_exists(cmd: str) -> bool:
    from shutil import which
    return which(cmd) is not None

def have_changed(in_files: List[str], out_file: str) -> bool:
    """Check if the output file needs to be rebuilt based on input files.
    
    The output file needs rebuilding if:
    - The output file doesn't exist
    - Any input file doesn't exist
    - Any input file is newer than the output file
    
    Args:
        in_files: List of input file paths (dependencies)
        out_file: Output file path to check
        
    Returns:
        True if out_file needs to be rebuilt, False otherwise
        
    Raises:
        ValueError: If in_files is empty
    """
    if not in_files:
        raise ValueError("Input files list cannot be empty")
    
    # Output file doesn't exist - needs rebuild
    out_path = Path(out_file)
    if not out_path.exists():
        return True
    
    try:
        out_mtime = out_path.stat().st_mtime
    except OSError as e:
        # Assume needs rebuild
        print(f"Warning: Cannot access {out_file}: {e}", file=sys.stderr)
        return True
    
    # Check each input file
    for in_file in in_files:
        in_path = Path(in_file)
        
        # Input file missing - needs rebuild
        if not in_path.exists():
            return True
        
        try:
            in_mtime = in_path.stat().st_mtime
            
            # Input file is newer - needs rebuild
            if in_mtime > out_mtime:
                return True
                
        except OSError as e:
            # Assume needs rebuild
            print(f"Warning: Cannot access {in_file}: {e}", file=sys.stderr)
            return True
    
    # All checks passed - no rebuild needed
    return False


def create_dirs(routes:List[str]):
    """Create directories if they don't already exist."""

    for r in routes:
        if(not os.path.exists(r)):
            os.makedirs(r)

def rm_all(path: str, excludes: List[str] = None):
    """Remove all files in the given directory (does not remove subdirectories).
    
    Args:
        path: Directory path to remove files from
        excludes: List of filenames to exclude from removal
        
    Raises:
        ValueError: If path doesn't exist or isn't a directory
    """
    if excludes is None:
        excludes = []
    
    if not os.path.exists(path):
        return
        
    
    if not os.path.isdir(path):
        raise ValueError(f"Path is not a directory: {path}")
    
    for f in os.listdir(path):
        file_path = os.path.join(path, f)
        
        if f in excludes or os.path.isdir(file_path):
            continue
            
        os.remove(file_path)


def create_statics(statics_dir: Union[str, Path], input_files: List[Union[str, Path]]) -> None:
    """
    Convert a list of binary files to C headers containing byte arrays.

    Args:
        statics_dir: Directory to write the generated .h files.
        input_files: List of binary file paths.

    Behavior:
        - Generates one .h file per input file in statics_dir
        - Skips missing files with a warning
        - Generates valid C identifiers as array names from file paths
    """
    statics_dir = Path(statics_dir)
    statics_dir.mkdir(parents=True, exist_ok=True)

    for file_path in input_files:
        file_path = Path(file_path)
        if not file_path.exists():
            print(f"Warning: Input file does not exist, skipping: {file_path}")
            continue

        array_name = file_path.name.replace("/", "_").replace("\\", "_").replace(".", "_").replace("-", "_")

        output_file = statics_dir / (file_path.stem + ".h")

        try:
            data = file_path.read_bytes()
        except OSError as e:
            print(f"Error reading file {file_path}: {e}")
            continue

        try:
            with output_file.open("w", encoding="utf-8") as f:
                f.write(f"// Generated from {file_path.name}\n")
                f.write(f"unsigned char {array_name}[] = {{\n")
                for i, b in enumerate(data):
                    if i % 12 == 0:
                        f.write("    ")
                    f.write(f"0x{b:02x}, ")
                    if i % 12 == 11:
                        f.write("\n")
                f.write("\n};\n\n")
                f.write(f"unsigned int {array_name}_len = {len(data)};\n")
        except OSError as e:
            print(f"Error writing header file {output_file}: {e}")


def __build_compile_command(
    cc: str,
    cflags: List[str],
    macros: Dict[str, str],
    includes: List[str],
    source: str,
    output: str
) -> List[str]:
    """Build the compilation command as a list of arguments.
    
    Returns:
        List of command arguments suitable for subprocess
    """
    command = [cc]
    command.extend(cflags)
    command.extend([f"-D{name}" + (f"={value}" if value else "") 
                    for name, value in macros.items()])
    command.extend([f"-I{inc}" for inc in includes])
    command.extend(["-c", "-o", output, source])
    
    return command   

def compile(
    cc: str,
    cflags: List[str],
    macros: Dict[str, str],
    includes: List[str],
    sources: List[str],
    out_dir: str,
    verbose: bool = True
) -> List[str]:
    """Compile C source files into object files, skipping unchanged files.
    
    Args:
        cc: C compiler command (e.g., 'gcc', 'clang')
        cflags: List of compiler flags
        macros: Dictionary of macro definitions (name -> value, empty string for no value)
        includes: List of include directories
        sources: List of source file paths
        out_dir: Output directory for object files
        verbose: Whether to print compilation commands
        
    Returns:
        List of object file paths (both newly compiled and skipped)
        
    Raises:
        ValueError: If sources list is empty or output directory cannot be created
        RuntimeError: If compilation fails for any source file
    """
    if not sources:
        raise ValueError("No source files provided")
    
    out_path = Path(out_dir)
    try:
        out_path.mkdir(parents=True, exist_ok=True)
    except Exception as e:
        raise ValueError(f"Failed to create output directory '{out_dir}': {e}")
    
    if not __command_exists(cc):
        raise ValueError(f"Compiler '{cc}' not found in PATH")
    
    out_files = []
    failed_sources = []
    
    for source in sources:
        source_path = Path(source)
        
        if not source_path.exists():
            print(f"{COLORS.RED}WARNING: Source file not found: {source}{COLORS.RESET}", 
                  file=sys.stderr)
            continue
        
        if not source_path.suffix == ".c":
            print(f"{COLORS.YELLOW}WARNING: Skipping non-C file: {source}{COLORS.RESET}")
            continue
        
        out_file = out_path / source_path.with_suffix(".o").name
        out_files.append(str(out_file))
        
        # Skip if unchanged
        if not have_changed([str(source_path)], str(out_file)):
            if verbose:
                print(f"{out_file} {COLORS.BLUE}-> up to date{COLORS.RESET}")
            continue
        
        # Build compilation command
        command = __build_compile_command(cc, cflags, macros, includes, 
                                         str(source_path), str(out_file))
        
        if verbose:
            print(f"{COLORS.GREEN}Compiling:{COLORS.RESET} {source_path.name}")
            print(f"  {' '.join(command)}")
        
        # Execute compilation
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                check=False,
                shell=True  # Add shell=True for Windows compatibility
            )
            
            if result.returncode != 0:
                failed_sources.append(source)
                print(f"{COLORS.RED}ERROR: Compilation failed for {source}{COLORS.RESET}", 
                      file=sys.stderr)
                if result.stderr:
                    print(result.stderr, file=sys.stderr)
                if result.stdout:
                    print(result.stdout, file=sys.stderr)
                
        except Exception as e:
            failed_sources.append(source)
            print(f"{COLORS.RED}ERROR: Failed to execute compiler: {e}{COLORS.RESET}", 
                  file=sys.stderr)
    
    # Report results
    if failed_sources:
        raise RuntimeError(
            f"Compilation failed for {len(failed_sources)} file(s): "
            f"{', '.join(failed_sources)}{COLORS.RESET}"
        )
    
    if verbose:
        print(f"{COLORS.GREEN}Successfully compiled {len(sources)} file(s)")
    
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



def zip_directory(src_dir: str | Path, zip_path:  str | Path) -> None:

    """
    Creates a ZIP file containing all files in src_dir and its subdirectories.

    Args:
        src_dir (str | Path): The source directory to be zipped.
        zip_path (str | Path): The path of the ZIP file to be created.

    Raises:
        FileNotFoundError: If src_dir does not exist.
        NotADirectoryError: If src_dir is not a directory.
        BadZipFile: If the ZIP file cannot be created.
        OSError: If there is a filesystem error while zipping the directory.
    """
    src_dir = Path(src_dir)
    zip_path = Path(zip_path)

    if not src_dir.exists():
        raise FileNotFoundError(f"Source directory does not exist: {src_dir}")
    
    if not src_dir.is_dir():
        raise NotADirectoryError(f"Source path is not a directory: {src_dir}")
    
    #ensure parent folder existance.
    zip_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        with zipfile.ZipFile(zip_path, "w", compression = zipfile.ZIP_DEFLATED) as zip:
            for root, dirs, files in os.walk(src_dir):
                for file in files: 
                    complete_path = Path(root) / file
                    arcname = complete_path.relative_to(src_dir)
                    zip.write(complete_path, arcname)

    except zipfile.BadZipFile as error:
        raise zipfile.BadZipFile(f"Failed to create ZIP file {zip_path}: {error}")
    except OSError as error:
        raise OSError(f"Filesystem error while zipping '{src_dir}' -> '{zip_path}': {error}")
    
if __name__ == "__main__":
    print(f"{COLORS.YELLOW}This file is not meant to be executed directly.{COLORS.RESET}")
    print(f"{COLORS.YELLOW}Please use build.py{COLORS.RESET}")