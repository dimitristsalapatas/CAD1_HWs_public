# CAD I Course Project

The tool is a Tcl console. `main()` in [cadI_project.cpp](cadI_project.cpp) registers the project's Tcl commands and starts the console, which is supported by a prebuilt tool called **ASP** (`libasp/libasp.so`).

* [Build](#build)
* [Run](#run)
* [Tcl commands](#tcl-commands)
* [Repository layout](#repository-layout)

---

## Build

```
cd src-liberty_parse-2.6
./configure
make clean
make
./makelib
cd ..
mkdir build
cd build
cmake ..
make
```

You need to install packages (based on your distro, package names may differ):

    cmake
    gperf
    libtcl8.6
    tcl8.6
    tcl8.6-dev
    gtk2-devel
    boost-devel
    libreadline
    git
    bison
    csh

Note: On some distros you may need to change `#include <tcl.h>` to `#include <tcl/tcl.h>` or `#include <tcl8.6/tcl.h>`, same for `tclDecls.h`.

## Run
An example of how to instantiate the Tcl console can be found in the [run.sh](run.sh)
```bash
cd build
export LD_LIBRARY_PATH=libasp
./cadI_project                                # interactive Tcl console
# or
./cadI_project ../test/template_script.tcl    # run a Tcl script
```

To familirize with the Tcl console you can use the `run_unit_test` command that creates a design manually (meaning without loading a netlist):

```tcl
set LIB_FILE     ../test/ihp013_openPDK.lib
set OUTPUT_NETLIST outputs/c17_out.v

load_liberty $LIB_FILE          ;# parse the Liberty file

run_unit_test                   ;# simple test for data structures
write_verilog $OUTPUT_NETLIST   
exit
```

Here is an example of how to load a design and output a netlist after the `create_data_structures` command is completed:

```tcl
set LIB_FILE     ../test/ihp013_openPDK.lib
set VERILOG_FILE ../test/netlists/c17.v
set OUTPUT_NETLIST outputs/c17_out.v

load_liberty $LIB_FILE          ;# parse the Liberty file
load_verilog $VERILOG_FILE

create_data_structures
write_verilog $OUTPUT_NETLIST   
exit
```


## Tcl commands

Already registered by this project in [cadI_project.cpp](cadI_project.cpp):

| Command | What it does |
|---|---|
| `load_liberty <file.lib>` | Parses the Liberty file into the `Lib` structures. |
| `create_data_structures` | Entry point where the netlist structures should be built. |
| `write_verilog <file>` | Writes the modules as a Verilog netlist in natural sorted order. |
| `run_unit_test` | Runs `unit_test_structs()` (see [Unit test](#unit-test)). |
| `get_cell_LUT <cell> <cell_rise\|cell_fall\|rise_transition\|fall_transition>` | Prints the timing arcs of a cell and its LUT values. |

### Unit test

`run_unit_test` ([structs/structs_unit.cpp](structs/structs_unit.cpp)) showcases an example of how data can be loaded in the project's structures.

## Repository layout

### Important files

| File | Purpose |
|---|---|
| [cadI_project.cpp](cadI_project.cpp) | Defines the Tcl commands and starts the console. |
| [cadI_project.hpp](cadI_project.hpp) | Header file. |
| [run.sh](run.sh) | Runs the Tcl console. |
| [run_valgrind.sh](run_valgrind.sh) | Runs the tool with Valgrind: `./run_valgrind.sh <executable> <tcl script> <suppression file> <log file>`. |
| [cad1_valgrind_suppress.supp](cad1_valgrind_suppress.supp) | Valgrind suppression list. |

### Source folders

| Folder | Purpose |
|---|---|
| [structs/](structs) | The design data structures. |
| [lib/](lib) | The library data structures. |
| [libasp/](libasp) | The binary of the ASP tool. |
| [src-liberty_parse-2.6/](src-liberty_parse-2.6) | The Liberty parser. |
| [test/](test) | Input Library and netlists. Also golden outputs will be provided to verify functionality for each assignment. |

#### `structs/`: design data structures

* [structs_def.hpp](structs/structs_def.hpp): the class definitions.
  * `Box` is the base of everything physical: x, y, height, width and origin.
  * `Instance` is a `Box` with a name and a parent. `Module` and `Cell` derive from it, and a union (`InstanceRef`) lets you go back from an `Instance` to the `Module` or `Cell`.
  * `Module` is a Verilog module. It owns its cells, child modules, ports and nets, so the module hierarchy is implicit in the tree.
  * `Cell` is an instance of a library cell. It owns its pins and keeps a pointer to its `LibCell`.
  * `Pin` belongs to a `Cell` and points to its `Net` and its `LibPin`.
  * `Port` belongs to a `Module`. A module port can have two nets attached: one that drives it and one it drives.
  * `Net` connects pins/ports with pins/ports, and keeps a bounding box.

Full names use `/` as the hierarchy separator and omit the top module name, e.g. `module1_inst/cell1/A`.

#### `lib/`: library data structures

[lib.hpp](lib/lib.hpp) and [lib.cpp](lib/lib.cpp) define structures like `Lib`, `LibCell`, `LibPin`, `LibTimingArc`, `LUT`. The data stored for each pin are: 
* Direction (input, output or inout)
* Capacitance range (input pins)
* Function (output pins)
* Timing arcs (output pins)

Each timing arc carries lookup tables (`cell_rise`, `cell_fall`, `rise_transition`, `fall_transition`), indexed by input slew and output load.