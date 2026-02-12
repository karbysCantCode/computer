#include <iostream>
#include <filesystem>

// execute on MAC with: ./exeName Args

//args
  //--smake smakeFilePath
  //  executes the smake file (mutually exclusive to --asm)
  //--asm spasmFilePath
  //  compiles the spasm file (mutually exclusive to --smake)
  //-v
  //  dumps warnings
  //-vv
  //  dumps warnings and debugs
  //-s
  //  mutes errors

//-1 == conflicting flags
//-2 == file not found
//-3 == no args
int main(int argc, char* argv[]) {
  std::cout << __cplusplus << std::endl;
  //static_assert(__cpp_lib_filesystem == 201703L, "PLEASE");
  std::cout << "Started with " << argc << " args" << std::endl;
  bool smakeFlag = false;
  bool asmFlag = false;

  std::cout << "ok";

  bool readErrors = true;
  bool readWarnings = false;
  bool readDebugs = false;

  std::filesystem::path smakePath;
  std::filesystem::path asmPath;

  std::cout << "ok" << std::endl;
  for (int i = 1; i < argc; i++) {
    std::cout << "i" << i << std::endl;
    std::cout << "i" << i << std::endl;
    std::cout << "i" << i << std::endl;
    const std::string arg = argv[i];
    if (arg == "--smake") {
      if (asmFlag) {std::cerr << "--smake mutually exclusive to --asm, aborted."; return -1;}
      smakeFlag = true;

      i++;
      if (i < argc) {
        smakePath = argv[i];
        if (!std::filesystem::exists(smakePath)) {std::cerr << "file \"" << smakePath.generic_string() << "\" does not exist. Aborted."; return -2;}
      }
    } else if (arg =="--asm") {
      if (smakeFlag) {std::cerr << "--asm mutually exclusive to --smake, aborted."; return -1;}
      asmFlag = true;

      i++;
      if (i < argc) {
        asmPath = argv[i];
        if (!std::filesystem::exists(asmPath)) {std::cerr << "file \"" << smakePath.generic_string() << "\" does not exist. Aborted."; return -2;}
      }
    } else if (arg =="-v") {
      readWarnings = true;
    } else if (arg =="-vv") {
      readDebugs = true;
    } else if (arg =="-s") {
      readErrors = false;
    } else {
      std::cout << "Unknown arg \"" << arg << "\"" << std::endl;
    }
  }

  std::cout <<"eel";
  std::cout <<"eel";
  if (!smakeFlag && !asmFlag) {
    asmFlag = true;
    if (argc < 2) {std::cerr << "No arguments passed, aborted."; return -3;}
    if (!(std::filesystem::exists(argv[1]))) {std::cerr << "file \"" << argv[1] << "\" does not exist. Aborted."; return -2;}
    asmPath = argv[2];
  }





  return 0;
}