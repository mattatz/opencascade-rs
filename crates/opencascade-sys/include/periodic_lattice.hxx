#pragma once
// Periodic lattice assembly (see src/periodic_lattice.cpp): the entry points
// the cxx bridge calls.

#include "rust/cxx.h"

#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <cstdint>
#include <memory>

// Instance the periodic `cell` (occupying `bounds`) over `cells`, given as
// (i, j, k) offsets; `reps` / `offsets` describe which result faces are
// translated copies of earlier ones.
std::unique_ptr<TopoDS_Shape> assemble_periodic_cells(const TopoDS_Shape &cell, rust::Slice<const double> bounds,
                                                      rust::Slice<const int32_t> cells, double tolerance,
                                                      rust::String &report, rust::Vec<int32_t> &reps,
                                                      rust::Vec<double> &offsets);

// As `assemble_periodic_cells`, keeping only the material inside `body`
// (`keep_outside` = false) or outside it (true).
std::unique_ptr<TopoDS_Shape> clip_periodic_cells(const TopoDS_Shape &cell, rust::Slice<const double> bounds,
                                                  rust::Slice<const int32_t> cells, const TopoDS_Shape &body,
                                                  bool keep_outside, double tolerance, rust::String &report,
                                                  rust::Vec<int32_t> &reps, rust::Vec<double> &offsets);

// As `assemble_periodic_cells`, with every cell's geometry moved by the
// displacement callbacks (`displace_fn`, `deviation_fn`, `trace_fn` are
// `extern "C"` function pointers taking `ctx` first).
std::unique_ptr<TopoDS_Shape> assemble_periodic_cells_displaced(
    const TopoDS_Shape &cell, rust::Slice<const double> bounds, rust::Slice<const int32_t> cells, double tolerance,
    size_t displace_fn, size_t deviation_fn, size_t trace_fn, size_t ctx, rust::String &report,
    rust::Vec<int32_t> &reps, rust::Vec<double> &offsets);

// As `clip_periodic_cells`, with the displacement callbacks.
std::unique_ptr<TopoDS_Shape> clip_periodic_cells_displaced(
    const TopoDS_Shape &cell, rust::Slice<const double> bounds, rust::Slice<const int32_t> cells,
    const TopoDS_Shape &body, bool keep_outside, double tolerance, size_t displace_fn, size_t deviation_fn,
    size_t trace_fn, size_t ctx, rust::String &report, rust::Vec<int32_t> &reps, rust::Vec<double> &offsets);

// Widen vertex tolerances to reach the ends of their edges' curves.
void heal_vertex_tolerances(const TopoDS_Shape &shape);

// A point strictly inside `solid`; throws when none is found.
std::unique_ptr<gp_Pnt> point_inside_solid(const TopoDS_Shape &solid, double tolerance);

bool brep_check_valid(const TopoDS_Shape &shape);

// BRepCheck's statuses of a shape, or "valid".
rust::String brep_check_report(const TopoDS_Shape &shape);

// Edges referenced by exactly one face (degenerated edges excluded).
int32_t count_free_edges(const TopoDS_Shape &shape);

// The solid bounded by a closed shell, reversed when the caller knows the
// shell faces inward (it keeps the orientation of the mesh it came from).
std::unique_ptr<TopoDS_Shape> solid_from_closed_shell(const TopoDS_Shape &shell, bool reverse);

// The solid bounded by `outer` with `cavities` as holes, every shell taken
// with the orientation it has (reversed when asked), no classification.
std::unique_ptr<TopoDS_Shape> solid_with_cavities(const TopoDS_Shape &outer, const TopTools_ListOfShape &cavities,
                                                  bool reverse);
