//   ____    _    ____    ___ ___    ____                            ____            _           _
//  / ___|  / \  |  _ \  |_ _|_ _|  / ___|___  _   _ _ __ ___  ___  |  _ \ _ __ ___ (_) ___  ___| |_
// | |     / _ \ | | | |  | | | |  | |   / _ \| | | | '__/ __|/ _ \ | |_) | '__/ _ \| |/ _ \/ __| __|
// | |___ / ___ \| |_| |  | | | |  | |__| (_) | |_| | |  \__ \  __/ |  __/| | | (_) | |  __/ (__| |_
//  \____/_/   \_\____/  |___|___|  \____\___/ \__,_|_|  |___/\___| |_|   |_|  \___// |\___|\___|\__|
//                                                                                |__/
//
// A project by University of Thessaly CASlab and Stavros Simoglou, for the
// purpose of academic training on the topics of physical design algorithms, EDA
// CAD, and larger project development
//
// We hope this project will spark your interest in the above topics and that
// you will enjoy the assignments.
//
// Happy Hacking!
//
// File:    cadI_project.cpp
// Purpose: main CAD I project entry point
// Authors: Stavros Simoglou, Dimitris Tsalapatas
//
#include <iostream>
#include <structs.hpp>
#include <tcl.h>
#include <string.h>
#include <tclDecls.h>
#include <iostream>
#include <filesystem>
#include "cadI_project.hpp"

int load_liberty(ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv) {
  char *libfilename = NULL;

  if (objc != 2) {
    Tcl_SetResult(interp, (char *)"Usage: load_liberty <libfile>", TCL_STATIC);
    return TCL_ERROR;
  }

  libfilename = Tcl_GetString((Tcl_Obj *) objv[1]);

  // replace a previously loaded library, unless a design already points
  // into it (design cells keep LibCell pointers)
  if (mainLibrary != nullptr) {
    if (topModule != nullptr) {
      Tcl_SetResult(interp, (char *)"Error. Cannot reload liberty after the design structures are created.", TCL_STATIC);
      return TCL_ERROR;
    }
    delete mainLibrary;
    mainLibrary = nullptr;
  }

  int ret = parse_liberty(libfilename, &mainLibrary);
  if (mainLibrary == NULL) {
    Tcl_SetResult(interp, (char *)"Error parsing liberty file", TCL_STATIC);
    return TCL_ERROR;
  }
  mainLibrary->print();
  mainLibrary->write_lef_for_ASP("/tmp/asp.lef");
  tclwrapeval((char *)"load_lef /tmp/asp.lef");

  return ret;
}

int write_verilog(ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv) {
  char *outputfile = NULL;
  string filename;

  if (objc != 2) {
    Tcl_SetResult(interp, (char *)"Usage: write_verilog <output file>", TCL_STATIC);
    return TCL_ERROR;
  }

  outputfile = Tcl_GetString((Tcl_Obj *) objv[1]);
  filename = string(outputfile);

  if (topModule == NULL) {
    Tcl_SetResult(interp, (char *)"Error. Top module is not defined.", TCL_STATIC);
    return TCL_ERROR;
  }

  topModule->write_netlist(filename);

  return TCL_OK;
}

int run_unit_test(ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv) {

  unit_test_structs();

  return TCL_OK;
}

int get_cell_LUT(ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv) {

  if (objc != 3) {
    Tcl_SetResult(interp, (char *)"Usage: get_cell_LUT <cell_name> <lut_type: cell_rise, cell_fall, rise_transition, fall_transition, rise_constraint, fall_constraint>", TCL_STATIC);
    return TCL_ERROR;
  }

  if (mainLibrary == nullptr) {
    Tcl_SetResult(interp, (char *)"Error. Main Library is not loaded.", TCL_STATIC);
    return TCL_ERROR;
  }

  char *cell_name = Tcl_GetString((Tcl_Obj *) objv[1]);
  char *lut_type_str = Tcl_GetString((Tcl_Obj *) objv[2]);

  LibCell *cell = mainLibrary->getCellByName(string(cell_name));
  if (!cell) {
    Tcl_SetResult(interp, (char *)"Error. Cell not found in library.", TCL_STATIC);
    return TCL_ERROR;
  }

  LUTType lut_type = stringToLUTType(lut_type_str);
  if (lut_type == (LUTType)NONE) {
    Tcl_SetResult(interp, (char *)"Error. Unknown LUT type.", TCL_STATIC);
    return TCL_ERROR;
  }

  // delay/slew LUTs live on output pins, constraint LUTs on input pins
  DirectionType pin_type = isConstraint(lut_type) ? INPUT : OUTPUT;

  for (auto pin : cell->getPins()) {
    if (pin.second->getType() != pin_type) {
      continue;
    }
    TimingInfo *t_info = pin.second->getTimingInfo();
    vector<LibTimingArc *> &arcs = (pin_type == INPUT)
                                       ? t_info->in_tinfo->constraint_arcs
                                       : t_info->out_tinfo->timing_arcs;

    cout << "->Pin: " << pin.second->getName() << endl;
    for (auto arc : arcs) {
      LUT *lut = arc->getLUTByType(lut_type);
      // not every arc has every table (e.g. clear arcs only have cell_fall)
      if (!lut) {
        continue;
      }
      long double *index_1 = nullptr;
      long double *index_2 = nullptr;
      long double *values = nullptr;
      int index1_size = 0;
      int index2_size = 0;
      getLUTDataFromTimingArc(arc, lut_type, &index_1, &index1_size,
                              &index_2, &index2_size, &values);

      // getLUTDataFromTimingArc returns the slew axis first, whatever the
      // template's variable order is
      LUTVariableType var1 = lut->getTemplate()->getVar1();
      LUTVariableType var2 = lut->getTemplate()->getVar2();
      if (var1 != INPUT_SLEW && var1 != CONSTRAINED_SLEW) {
        swap(var1, var2);
      }

      arc->print();
      cout << LUTVariableTypeToString(var1) << ": ";
      for (int i = 0; i < index1_size; i++) {
        cout << index_1[i] << ", ";
      }
      cout << endl;
      cout << LUTVariableTypeToString(var2) << ": ";
      for (int i = 0; i < index2_size; i++) {
        cout << index_2[i] << ", ";
      }
      cout << endl;
      cout << "values: ";
      for (int i = 0; i < index1_size*index2_size; i++) {
        cout << values[i] << ", ";
      }
      cout << endl;
    }
  }

  return TCL_OK;
}

int main(int argc, char *argv[]) {

  char *arguments = NULL;
  std::cout << "Init TCL" << std::endl;

  // allocate tcl command names
  cadII_tcl_commands_num = 5;
  cmdNames    = (char **) calloc(cadII_tcl_commands_num, sizeof(char *));
  cmdNames[0] = strdup("create_data_structures");
  cmdNames[1] = strdup("load_liberty");
  cmdNames[2] = strdup("write_verilog");
  cmdNames[3] = strdup("run_unit_test");
  cmdNames[4] = strdup("get_cell_LUT");


  procs    = (Tcl_ObjCmdProc **)calloc(cadII_tcl_commands_num,
                                       sizeof(Tcl_ObjCmdProc *));
  procs[0] = (Tcl_ObjCmdProc *) create_data_structures;
  procs[1] = (Tcl_ObjCmdProc *) load_liberty;
  procs[2] = (Tcl_ObjCmdProc *) write_verilog;
  procs[3] = (Tcl_ObjCmdProc *) run_unit_test;
  procs[4] = (Tcl_ObjCmdProc *) get_cell_LUT;


  if (argc > 1) {
    arguments = argv[1];
    if (!filesystem::exists(arguments)) {
      cout << "File " << arguments << " does not exist!." << endl;
      cout << "Starting interactive console." << endl;

      arguments = NULL;
    }
  }
  else {
    arguments = NULL;
  }
  start_console(arguments);

  cout << "Exiting CAD I Project." << endl;

  return 1;
}
