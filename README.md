# Diannex
Diannex is a text and dialogue language, designed to assist in the creation of scenes (with branching paths) and localization for games, as well as other applications.

This repository contains the universal tool for working with Diannex files: it is primarily a compiler and project generator, but also a converter.
  
## About
Diannex operates on a per-project basis, requiring either an (auto-generated) JSON project file, or arguments given via command line. Script files are used to write code in, and then the project can be run through this compiler, generating an output binary file. Optionally, "translation files," with strings to be localized, can be generated. These translation files have a public and private variant (private ones containing more information), for ease of localization. These files (excluding private translation files specifically) are meant to be parsed and interpreted by a host application.

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

## Further documentation
TODO

## Usage
The tool is a command-line application, with these options which can be seen simply by running with `--help`.
```
  diannex [OPTION...] <files>

  -p, --project arg             Load project file
  -g, --generate [=arg(=DiannexTesting)]
                                Generate new project file
      --convert                 Convert a private file to the public format
  -c, --cli                     Don't use a project file and read commands
                                from cli
  -h, --help                    Shows this message

 Conversion options:
      --in arg   Path to private input file
      --out arg  Path to public output file

 Project options:
  -b, --binary (default: "./out")
                                Directory to output binary
  -n, --name (default: "out")   Name of output binary file
  -t, --public                  Whether to output public translation file
  -N, --pubname (default: "out")
                                Name of output public translation file
  -T, --private                 Whether to output private translation files
  -D, --privname (default: "out")
                                Name of output private translation file
  -d, --privdir (default: "./translations")
                                Directory to output private translation files
  -C, --compress                Whether or not to use compression
  --files                       File(s) to compile
  ```
  
## Building
This project uses [CMake](https://cmake.org/) to compile, but it also requires a C++ compiler with non-experimental C++17 support. (C++17 classes shouldn't be in the `std::experimental` namespace)

If you have these requments satisfied, then you should just be able to compile with the following commands:
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
