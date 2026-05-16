#define hello(NAME) \
      a NAME, c;

int main() {
  typedef char (*a)[10];
  hello(b);
  char* d = b[0];
  a e, *f;
};

const volatile extern int e;
struct r {
  int b;
};
const const int* const i;
unsigned r;

const int(* i())[10]{
  int b[10];
  return &b;
}

//storage class lim 1
//kw ty lim 1

/*
/usr/bin/clang++ 
-DRELATIVE_FILE_PREFIX="" 
-I"/Users/karbys/logisim projects/16bitcomputer/CppCompiler/src/src3" 
-g 
-std=gnu++20 
-arch arm64 
-MD 
-MT 
CMakeFiles/spasm.dir/src/src3/Helpers/CLIOptions.cpp.o 
-MF 
CMakeFiles/spasm.dir/src/src3/Helpers/CLIOptions.cpp.o.d 
-o CMakeFiles/spasm.dir/src/src3/Helpers/CLIOptions.cpp.o 
-c "/Users/karbys/logisim projects/16bitcomputer/CppCompiler/src/src3/Helpers/CLIOptions.cpp"



*/