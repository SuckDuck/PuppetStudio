#! /usr/bin/python
from littlebuild import *

project_title = "PuppetStudio"
project_version = "1.0.0"

def compress():
    zip_directory("sampleProject", "sampleProject/sampleProject.zip")
    zip_directory("puppets", "puppets/samplePuppets.zip")
    
def statics():
    compress()
    
    ASSETS_DIR = "assets"
    STATICS_DIR = "statics"
    create_dirs([STATICS_DIR])

    input_files = [str(Path(ASSETS_DIR) / f) for f in os.listdir(ASSETS_DIR)]
    input_files += ["sampleProject/sampleProject.zip", "puppets/samplePuppets.zip"]
    create_statics(STATICS_DIR,input_files)

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
    CFLAGS =  ["-g", "-O0"]
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
        "PROJECT_TITLE":f'\"{project_title}\"', 
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


def run_web():

    from http.server import HTTPServer, SimpleHTTPRequestHandler

    class CORSRequestHandler(SimpleHTTPRequestHandler):
        def end_headers(self):
            self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
            self.send_header('Cross-Origin-Opener-Policy', 'same-origin')

            # Ensure correct MIME types
            super().end_headers()
        
        def guess_type(self, path):
            mimetype = super().guess_type(path)
            if path.endswith('.wasm'):
                return 'application/wasm'
            
            return mimetype
    
    os.chdir("build/web")
    server = HTTPServer(('localhost', 8000), CORSRequestHandler)
    print(f'{COLORS.GREEN}Server running at http://localhost:8000{COLORS.RESET}')
    print(f'{COLORS.GREEN}Press Ctrl+C to stop{COLORS.RESET}')
    server.serve_forever()

if __name__ == "__main__":

    if "clean" in sys.argv:
        clean()
    elif "clear" in sys.argv:
        clean()
    
    statics()

    if "web" in sys.argv:
        web()
    elif "linux" in sys.argv:
        linux()

    #To run the program we need to deploy a quick webserver due to CORS security policy
    if "run-web" in sys.argv:
        run_web()