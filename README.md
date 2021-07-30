# Diannex
Diannex is a text and dialogue language, designed to assist in the creation of scenes (with branching paths) and localization for games, as well as other applications.

This repository contains the universal tool for working with Diannex files: it is primarily a compiler and project generator, but also a converter.
  
## About
Diannex operates on a per-project basis, requiring either an (auto-generated) JSON project file, or arguments given via command line. Script files (.dx extension) are used to write code in, and then the project can be run through this compiler, generating an output binary file. Optionally, "translation files," with strings to be localized, can be generated. These translation files have a public and private variant (private ones containing more information), for ease of localization. These files (excluding private translation files specifically) are meant to be parsed and interpreted by a host application.

## Your first project
1. Download the newest release of Diannex (or build it yourself).
2. Export Diannex to a location on your computer.
3. Open the command prompt and navigate to the directory where you exported Diannex (or add it to your PATH).
4. Execute `diannex --generate` to generate a new project. You can rename the file if you wish.
5. Create a new file in the same directory as the generated project file called "myfile.dx" and add the code from [Appendix A](#a---hello-world). Save the file.
7. Open the generated project file and add the relative path to "myfile.dx" (if it's in the same directory, "myfile.dx" is enough) to the "files" array. Save the file.
8. Go back to the command prompt and execute `diannex --project <project file name>` to create the binary file. More usage [here](#usage).
9. Congratulations! You now have a binary file which can be loaded into your game or application.

## Basic language sample
```c
//! Main menu
def menu
{
  button_start="Start"
  button_continue="Continue"
  button_quit="Quit"
  label_welcome="Welcome, ${getPlayerName()}!"
}

//! First area of the game
namespace area0
{
  scene intro
  {
    narrator: "Welcome to the test introduction scene!"
    "One quick thing I have to ask before you begin..."
    choice "Is this a question?"
    {
      "Yes"
      {
        "That is correct."
        awardPoints 1
      }
      "No"
      {
        "Hm... I don't believe that's correct."
        deductPoints true, 5
      }
    }
    "Either way, it was nice meeting you, ${getPlayerName()}."
    "This is the end of the sample intro scene!"
    
    if (getFlag("sample") == 1)
      "By the way, I feel like I've said this before... quite strange."
    else
      setFlag "sample", 1
      
    // By the way, this is a normal comment, which only appears in *this* code, and never in a private translation file
    //! (this appears in private translation files)
    /*! (this also appears
         in private translation files) */
    
    "Well, now it's time for a loop!"
    for (local $i = 0; $i < 5; $i++)
      example($i)
    
    "Or, a simpler loop!"
    //! This line is repeated 5 times
    repeat (5)
      "The same thing, over and over..."
  }

  func example(a)
  {
    choose
    {
      "This is an example function, being passed ${$a}"
      "This is an example function (but an alternate line with 50% chance), being passed ${$a}"
    }
  }
}
```

## Binary format
```
Header:
 header - DNX
 version - UInt8

Flags:
 flags - UInt8
  compressed - 0th bit from the right
  internalTranslationFile - 1st bit from the right

size - UInt32

if flag->compressed
 compressedSize - UInt32
 
 !! Everything following this is compressed with zlib (C++ Miniz) !!

Scene metadata:
 size - UInt32
 data[size]
  symbol - UInt32
  indicesSize - UInt16
  indices[indicesSize]
   instructionOffset - Int32

Function metadata:
 size - UInt32
 data[size]
  symbol - UInt32
  indicesSize - UInt16
  indices[indicesSize]
   instructionOffset - Int32

Definition metadata:
 size - UInt32
 data[size]
  symbol - UInt32
  reference - UInt32
   internal - 31st bit from the right
  instructionOffset - Int32

Bytecode:
 size - UInt32
 data[size]
  opcode - UInt8
  
  if opcode=0x0A(freeloc), 0x10(pushi), 0x12(pushs), 0x14(pushbs), 0x19(setvarglb), 0x1A(setvarloc), 0x1B(pushvarglb), 0x1C(pushvarloc), 0x40(j), 0x41(jt), 0x42(jf), 0x48(choiceadd), 0x49(choiceaddt), 0x4B(chooseadd), 0x4C(chooseaddt), 0x16(makearr)
   arg - Int32
  elif opcode=0x45(call), 0x46(callext), 0x13(pushints), 0x15(pushbints)
   arg - Int32
   arg2 - Int32
  elif opcode=0x11(pushd)
   arg - Double

Internal string table:
 size - UInt32
 data[size]
  string - String
 
if flag->internalTranslationFile
 Internal translation file:
  size - UInt32
  data[size]
   translationInfo - String
 
External function list:
 size - UInt32
 data[size]
  ID - UInt32
```

## Usage
The tool is a command-line application, with these options which can be seen simply by running with `--help`.
```
  diannex [OPTION...] <files>

  -p, --project <path>                         Load project file
  -g, --generate[=path(=DiannexTesting)]       Generate new project file
      --convert                                Convert a private file to the public format
  -c, --cli                                    Don't use a project file and read commands from cli
  -h, --help                                   Shows this message

 Conversion options:
      --in <path>                              Path to private input file
      --out <path>                             Path to public output file

 Project options:
  -b, --binary (default: "./out")              Directory to output binary
  -n, --name (default: "out")                  Name of output binary file
  -t, --public                                 Whether to output public translation file
  -N, --pubname (default: "out")               Name of output public translation file
  -T, --private                                Whether to output private translation files
  -D, --privname (default: "out")              Name of output private translation file
  -d, --privdir (default: "./translations")    Directory to output private translation files
  -C, --compress                               Whether or not to use compression
  --files[=path,path...]                       File(s) to compile
  ```
  
## Building
This project uses [CMake](https://cmake.org/) to compile, but it also requires a C++ compiler with non-experimental C++17 support. (C++17 classes shouldn't be in the `std::experimental` namespace)

If you have these requirments satisfied, then you should just be able to compile with the following commands:
```zsh
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
```

If you're using a compiler that requires linking a library for std::filesystem (for example, a GCC version less than GCC 9), set either the `LINK_LIBSTD_FS` or `LINK_LIBCPP_FS` flags to link with `-lstdc++fs` or `-lc++fs` respectively.

## Libraries
[jarro2783/cxxopts](https://github.com/jarro2783/cxxopts) is licensed under the [MIT License](https://github.com/jarro2783/cxxopts/blob/master/LICENSE).

[nlohmann/json](https://github.com/nlohmann/json) is licensed under the [MIT License](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT).

[agauniyal/rang](https://github.com/agauniyal/rang) is [Unlicensed](https://github.com/agauniyal/rang/blob/master/LICENSE).

[richgel999/miniz](https://github.com/richgel999/miniz) is licensed under the [MIT License](https://github.com/richgel999/miniz/blob/master/LICENSE).

## Contributors
[colinator27](https://github.com/colinator27)

[PeriBooty](https://github.com/PeriBooty)

[MadCreativity](https://github.com/aam051102)

## Appendix
### A - Hello world
```c
scene intro {
  narrator: "Hello, world."
}
```
