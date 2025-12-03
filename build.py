#! /usr/bin/python
from littlebuild import *

project_title = "PuppetStudio"
project_version = "1.0.0"

def compress():
    os.system("zip -r sampleProject/sampleProject.zip sampleProject/*")
    os.system("zip -r puppets/samplePuppets.zip puppets/*")

def statics():
    compress()
    ASSETS_DIR = "assets"
    STATICS_DIR = "statics"
    create_dirs([STATICS_DIR])
    create_statics(STATICS_DIR,[f"{ASSETS_DIR}/{i}" for i in os.listdir(ASSETS_DIR)] + ["sampleProject/sampleProject.zip", "puppets/samplePuppets.zip"])

def linux():
    CC = "gcc"
    CFLAGS =  ["-g -O0"]
    BUILD_DIR = "build/linux"
    SRC_DIR = "src"
    INCLUDES = ["include","statics"]
    LIB_PATHS = []
    LINKS = ["-lm","-lraylib"]
    LDFLAGS = []
    MACROS = {
        "PROJECT_TITLE":f'\\"{project_title}\\"', 
        "PROJECT_VERSION":int(project_version.replace(".",""))
    }
    
    create_dirs([BUILD_DIR])
    objs = compile(CC,CFLAGS,MACROS,INCLUDES,[f"{SRC_DIR}/{i}" for i in os.listdir(SRC_DIR)],out_dir=BUILD_DIR)
    link(CC,objs,LIB_PATHS,LINKS,LDFLAGS,BUILD_DIR,project_title)

def web():
    CC = "emcc"
    CFLAGS =  ["-g -O0"]
    BUILD_DIR = "build/web"
    SRC_DIR="src"
    INCLUDES = ["include","statics"]
    LIB_PATHS = ["-Llib"]
    LINKS = ["-lm","-lraylib.web"]
    LDFLAGS = [
        "-sUSE_GLFW=3", 
        "-sINITIAL_MEMORY=32mb", 
        "-sALLOW_MEMORY_GROWTH=1",
        "-sASYNCIFY", 
        '-sEXPORTED_FUNCTIONS=["_main","_PushLogSimple"]', 
        "-sEXPORTED_RUNTIME_METHODS=['ccall']", 
        "-DPLATFORM_WEB"
    ]

    MACROS = {
        "PROJECT_TITLE":f'\\"{project_title}\\"', 
        "PROJECT_VERSION":int(project_version.replace(".",""))
    }

    create_dirs([BUILD_DIR])
    objs = compile(CC,CFLAGS,MACROS,INCLUDES,[f"{SRC_DIR}/{i}" for i in os.listdir(SRC_DIR)],out_dir=BUILD_DIR)
    link(CC,objs,LIB_PATHS,LINKS,LDFLAGS,BUILD_DIR,"index.js")

def clean():
    rm_all("build/linux")
    rm_all("build/web",["index.html", "fflate_min.js"])
    rm_all("statics")
    if (os.path.exists("sampleProject/sampleProject.zip")): os.remove("sampleProject/sampleProject.zip")
    if (os.path.exists("puppets/samplePuppets.zip")): os.remove("puppets/samplePuppets.zip")

if __name__ == "__main__":
    if   "clean" in sys.argv: clean(); exit(0)
    elif "clear" in sys.argv: clean(); exit(0)
    statics()
    if "web" in sys.argv: web(); exit(0)
    else: linux()
