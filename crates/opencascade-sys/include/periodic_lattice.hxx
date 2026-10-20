// Periodic cell assembly: instance one B-rep unit cell over a set of lattice
// cells and share the boundary topology between neighbours by construction
// (no sewing, no geometric matching across cells).
//
// The template cell lives in the box `bounds`; its sub-shapes on a min face of
// the box must have a translated twin on the opposite max face (the cell is
// periodic). Each boundary vertex / edge is mapped to that canonical twin plus
// a cell shift, so instancing cell (i,j,k) looks the shared sub-shape up under
// the neighbour's key and reuses it. Planar faces lying on the box faces are
// caps: they are emitted only on sides without a neighbour cell.
//
// Clipping against a body runs one boolean per cell the body surface touches
// (cells fully inside / outside are kept whole or dropped). The topology the
// clipped cells share (their interface faces with edges and vertices) is
// split by the body once beforehand, so every cell's boolean starts from the
// same split pieces and keeps them; the per-cell results are then stitched
// through the booleans' history: splits and generated sub-shapes of a shared
// original are matched between neighbours and substituted with
// BRepTools_ReShape, so the clipped cells share their interface topology
// like the unclipped ones.
#pragma once

#include "rust/cxx.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck_ListIteratorOfListOfStatus.hxx>
#include <BRepCheck_Result.hxx>
#include <BRepClass3d.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepTools.hxx>
#include <BRepLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools_ReShape.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Geom2dAPI_Interpolate.hxx>
#include <Geom2dConvert.hxx>
#include <TColStd_HArray1OfReal.hxx>
#include <TColgp_HArray1OfPnt2d.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Surface.hxx>
#include <OSD_Parallel.hxx>
#include <Standard_Failure.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <Precision.hxx>
#include <ShapeFix_Edge.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TopAbs.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax22d.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <iostream>
#include <cstdint>
#include <map>
#include <string>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

inline std::unique_ptr<gp_Pnt> point_inside_solid(const TopoDS_Shape &solid, double tolerance);
inline rust::String brep_check_report(const TopoDS_Shape &shape);

namespace periodic_lattice {

// Closed shells nested by containment: a shell at odd depth is a cavity of
// the shell directly around it, one at even depth bounds a solid of its own.
// `known[i]`: -2 when shell i must be classified, -1 when it is known to bound
// a solid, else the index of the known shell it is a cavity of.
inline std::vector<TopoDS_Shape> nest_shells(const std::vector<TopoDS_Shell> &shells, double tolerance,
                                             const std::vector<int> &known = {}) {
  BRep_Builder builder;
  const size_t n = shells.size();
  std::vector<TopoDS_Solid> plain(n);
  std::vector<double> volumes(n, 0.0);
  std::vector<bool> cavity(n, false);
  std::vector<Bnd_Box> boxes(n);
  // The faces keep the orientation of the solids they were cut from, so a
  // shell enclosing void (a cavity, or a pocket spanning cells) has negative
  // volume as it stands.
  const auto volumes_started = std::chrono::steady_clock::now();
  int oriented = 0;
  for (size_t i = 0; i < n; ++i) {
    // The faces keep the orientation of the solids they were cut from: a
    // known cavity shell points into its void and a known solid shell
    // outward. Only a shell of unknown standing is tested, by whether
    // orienting it as a closed solid reverses it.
    builder.MakeSolid(plain[i]);
    if (known.size() == n && known[i] >= 0) {
      cavity[i] = true;
      builder.Add(plain[i], shells[i].Reversed());
    } else {
      builder.Add(plain[i], shells[i]);
      if (known.size() == n && known[i] == -1) {
        cavity[i] = false;
      } else {
        const TopAbs_Orientation before = TopoDS_Iterator(plain[i]).Value().Orientation();
        BRepLib::OrientClosedSolid(plain[i]);
        cavity[i] = TopoDS_Iterator(plain[i]).Value().Orientation() != before;
        ++oriented;
      }
    }
    BRepBndLib::Add(plain[i], boxes[i], Standard_False);
    double x0, y0, z0, x1, y1, z1;
    boxes[i].Get(x0, y0, z0, x1, y1, z1);
    volumes[i] = (x1 - x0) * (y1 - y0) * (z1 - z0);
  }
  if (std::getenv("NODI_TPMS_DEBUG") != nullptr && oriented > 0) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - volumes_started).count();
    std::cerr << "nest_shells: " << oriented << " of " << n << " shells oriented in " << seconds << "s" << std::endl;
  }
  // container[i]: the solid a cavity belongs to, from provenance when known,
  // else the smallest enclosing solid; classifiers are built once per solid.
  std::vector<int> container(n, -1);
  const auto started = std::chrono::steady_clock::now();
  int classified = 0;
  auto inside_box = [](const Bnd_Box &outer, const Bnd_Box &inner) {
    double x0, y0, z0, x1, y1, z1;
    inner.Get(x0, y0, z0, x1, y1, z1);
    return !outer.IsOut(gp_Pnt(x0, y0, z0)) && !outer.IsOut(gp_Pnt(x1, y1, z1));
  };
  std::unordered_map<size_t, std::unique_ptr<BRepClass3d_SolidClassifier>> classifiers;
  for (size_t i = 0; i < n; ++i) {
    if (!cavity[i]) {
      continue;
    }
    if (known.size() == n && known[i] >= 0 && !cavity[size_t(known[i])]) {
      container[i] = known[i];
      continue;
    }
    std::vector<size_t> candidates;
    for (size_t j = 0; j < n; ++j) {
      if (!cavity[j] && volumes[j] > volumes[i] && inside_box(boxes[j], boxes[i])) {
        candidates.push_back(j);
      }
    }
    if (candidates.size() == 1) {
      container[i] = int(candidates.front());
      continue;
    }
    if (candidates.empty()) {
      continue;
    }
    const gp_Pnt probe = *point_inside_solid(plain[i], tolerance);
    for (size_t j : candidates) {
      auto it = classifiers.find(j);
      if (it == classifiers.end()) {
        it = classifiers.emplace(j, std::make_unique<BRepClass3d_SolidClassifier>(plain[j])).first;
      }
      it->second->Perform(probe, tolerance);
      ++classified;
      if (it->second->State() == TopAbs_IN &&
          (container[i] < 0 || volumes[j] < volumes[size_t(container[i])])) {
        container[i] = int(j);
      }
    }
  }
  if (std::getenv("NODI_TPMS_DEBUG") != nullptr && classified > 0) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::cerr << "nest_shells: " << classified << " classifications over " << n << " shells in " << seconds << "s"
              << std::endl;
  }
  std::vector<TopoDS_Shape> out;
  for (size_t i = 0; i < n; ++i) {
    // A void shell with no solid around it stands alone, oriented outward.
    if (cavity[i] && container[i] >= 0) {
      continue;
    }
    TopoDS_Solid solid;
    builder.MakeSolid(solid);
    for (TopExp_Explorer ex(plain[i], TopAbs_SHELL); ex.More(); ex.Next()) {
      builder.Add(solid, ex.Current());
    }
    for (size_t c = 0; c < n; ++c) {
      if (cavity[c] && container[c] == int(i)) {
        for (TopExp_Explorer ex(plain[c], TopAbs_SHELL); ex.More(); ex.Next()) {
          builder.Add(solid, ex.Current().Reversed());
        }
      }
    }
    out.push_back(solid);
  }
  return out;
}

struct Cell {
  int i, j, k;
};

inline int64_t pack_cell(int i, int j, int k) {
  const int64_t off = int64_t(1) << 20;
  return ((int64_t(i) + off) << 42) | ((int64_t(j) + off) << 21) | (int64_t(k) + off);
}

struct InstanceKey {
  int canon;
  int64_t cell;
  bool operator==(const InstanceKey &o) const { return canon == o.canon && cell == o.cell; }
};

struct InstanceKeyHash {
  size_t operator()(const InstanceKey &k) const {
    return std::hash<int64_t>()(k.cell * 1000003 + k.canon);
  }
};

// Periodic classification of a template vertex or edge.
struct Boundary {
  int canon = -1;           // index of the canonical twin (self when canonical)
  int shift[3] = {0, 0, 0}; // 1 on axes where the sub-shape lies on the min face
  bool reversed = false;    // edge: own parameter runs opposite to the canonical one
  double delta = 0.0;       // edge: canonical parameter = own parameter + delta
  bool project = false;     // edge: twin parametrised differently, pcurves re-projected
  bool degenerated = false; // edge: no 3D curve, never shared
};

inline int count_shells(const TopoDS_Shape &shape) {
  int n = 0;
  for (TopExp_Explorer ex(shape, TopAbs_SHELL); ex.More(); ex.Next()) {
    ++n;
  }
  return n;
}

inline int32_t count_free_edges_of(const TopoDS_Shape &shape) {
  TopTools_IndexedDataMapOfShapeListOfShape map;
  TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, map);
  int32_t free_edges = 0;
  for (int i = 1; i <= map.Extent(); ++i) {
    const TopoDS_Edge &e = TopoDS::Edge(map.FindKey(i));
    if (BRep_Tool::Degenerated(e)) {
      continue;
    }
    if (map.FindFromIndex(i).Extent() == 1) {
      ++free_edges;
    }
  }
  return free_edges;
}

class Assembler {
public:
  Assembler(const TopoDS_Shape &cell, const double *bounds, double tol)
      : cell_(cell), tol_(tol) {
    for (int a = 0; a < 3; ++a) {
      min_[a] = bounds[a];
      max_[a] = bounds[3 + a];
      period_[a] = max_[a] - min_[a];
      if (!(period_[a] > 0.0)) {
        throw std::runtime_error("periodic cell box must have positive extent on every axis");
      }
    }
    classify_vertices();
    classify_edges();
    classify_faces();
  }

  // Instance `cells`; with `body`, keep only the material inside it
  // (`keep_outside` = false) or outside it (true).
  std::unique_ptr<TopoDS_Shape> assemble(const std::vector<Cell> &cells, const TopoDS_Shape *body,
                                         bool keep_outside, std::string &report) {
    for (const Cell &c : cells) {
      classes_.emplace(pack_cell(c.i, c.j, c.k), Class::Keep);
    }
    int warned = 0;
    std::vector<TopoDS_Shape> clipped;
    Stopwatch watch;
    if (body) {
      classify_cells(cells, *body, keep_outside);
      index_body_edges(*body);
      watch.lap("classify");
      // Every clipped cell is built before the first boolean splits a shared
      // edge: split edges copy the pcurves their parent has at that moment,
      // and must carry the pcurves of every neighbouring face.
      std::vector<std::pair<Cell, TopoDS_Shape>> solids;
      for (const Cell &c : cells) {
        if (classes_[pack_cell(c.i, c.j, c.k)] == Class::Clip) {
          solids.emplace_back(c, cell_solid(c));
        }
      }
      // Kept cells next to clipped ones are built now too, so that the
      // pre-split copies their pcurves along with the clipped cells'.
      for (const Cell &c : cells) {
        if (classes_[pack_cell(c.i, c.j, c.k)] == Class::Keep && touches_clip(c)) {
          std::vector<TopoDS_Face> &faces = kept_faces_[pack_cell(c.i, c.j, c.k)];
          for (int fi = 1; fi <= faces_.Extent(); ++fi) {
            if (cap_side_[fi - 1] < 0) {
              faces.push_back(instantiate_face(fi, c));
            }
          }
          for (int side = 0; side < 6; ++side) {
            side_faces(c, side);
          }
        }
      }
      presplit_interfaces(*body);
      for (auto &s : solids) {
        s.second = presplit_.Apply(s.second);
      }
      watch.lap("solids");
      // Booleans run in parallel over chunks (non-destructive: the inputs are
      // only read); their histories are then recorded in cell order.
      const size_t chunk = (std::max)(16, 4 * OSD_Parallel::NbLogicalProcessors());
      for (size_t start = 0; start < solids.size(); start += chunk) {
        const size_t end = (std::min)(solids.size(), start + chunk);
        std::vector<CellBoolean> algos(end - start);
        std::vector<std::string> errors(end - start);
        const bool serial = std::getenv("NODI_TPMS_SERIAL") != nullptr;
        const auto booleans_started = std::chrono::steady_clock::now();
        OSD_Parallel::For(
            0, int(end - start),
            [&](int i) {
              try {
                algos[i] = run_boolean(solids[start + i].second, *body, keep_outside);
              } catch (const Standard_Failure &e) {
                errors[i] = e.GetMessageString() ? e.GetMessageString() : "OCCT failure";
              } catch (const std::exception &e) {
                errors[i] = e.what();
              }
            },
            serial);
        watch.lap("booleans");
        if (std::getenv("NODI_TPMS_DEBUG") != nullptr) {
          report_booleans(algos, serial, booleans_started);
        }
        for (size_t i = start; i < end; ++i) {
          const Cell &c = solids[i].first;
          if (!errors[i - start].empty()) {
            std::ostringstream msg;
            msg << "periodic cell (" << c.i << ", " << c.j << ", " << c.k << ") clip failed: " << errors[i - start];
            throw std::runtime_error(msg.str());
          }
          Class &cls = classes_[pack_cell(c.i, c.j, c.k)];
          TopoDS_Shape result = record_cell(c, solids[i].second, algos[i - start], warned);
          if (!TopExp_Explorer(result, TopAbs_FACE).More()) {
            cls = Class::Drop;
            continue;
          }
          clipped.push_back(result);
        }
        watch.lap("record");
      }
      resolve_interfaces();
    }

    int kept = 0, cut = 0, dropped = 0;
    for (const Cell &c : cells) {
      const Class cls = classes_[pack_cell(c.i, c.j, c.k)];
      kept += cls == Class::Keep;
      cut += cls == Class::Clip;
      dropped += cls == Class::Drop;
      if (cls != Class::Keep) {
        continue;
      }
      auto built = kept_faces_.find(pack_cell(c.i, c.j, c.k));
      int k = 0;
      for (int fi = 1; fi <= faces_.Extent(); ++fi) {
        if (cap_side_[fi - 1] < 0) {
          const TopoDS_Face face = built == kept_faces_.end() ? instantiate_face(fi, c) : built->second[k++];
          for (const TopoDS_Face &f : presplit_faces(face)) {
            out_faces_.push_back(f);
            out_template_.emplace_back(fi, c);
            out_origin_.emplace_back(pack_cell(c.i, c.j, c.k), face_solid_[fi - 1]);
            out_cavity_.push_back(face_cavity_[fi - 1]);
            out_cap_.push_back(false);
          }
        }
      }
      for (int side = 0; side < 6; ++side) {
        const Class ncls = class_of(neighbour(c, side));
        if (ncls == Class::Keep) {
          continue;
        }
        for (const SideFace &sf : side_faces(c, side)) {
          if (ncls == Class::Drop || !interface_closed(sf.face)) {
            for (const TopoDS_Face &f : presplit_faces(sf.face)) {
              out_faces_.push_back(f);
              out_template_.emplace_back(sf.index, sf.owner);
              out_origin_.emplace_back(pack_cell(c.i, c.j, c.k), -1);
              out_cavity_.push_back(false);
              out_cap_.push_back(true);
            }
          }
        }
      }
    }

    if (!clipped.empty()) {
      TopoDS_Compound all;
      builder_.MakeCompound(all);
      for (const TopoDS_Shape &s : clipped) {
        builder_.Add(all, s);
      }
      const TopoDS_Shape stitched = reshape_.Apply(all);
      // Material kept inside a body whose outside is one connected region
      // reaches that region through every piece of the body's surface, so
      // a shell holding such a piece is an outer shell, like one with a cap.
      std::unordered_set<const Geom_Surface *> body_surfaces;
      if (!keep_outside && count_shells(*body) == 1) {
        for (TopExp_Explorer ex(*body, TopAbs_FACE); ex.More(); ex.Next()) {
          TopLoc_Location loc;
          body_surfaces.insert(BRep_Tool::Surface(TopoDS::Face(ex.Current()), loc).get());
        }
      }
      for (TopExp_Explorer ex(stitched, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face &f = TopoDS::Face(ex.Current());
        TopLoc_Location loc;
        const bool on_body = body_surfaces.count(BRep_Tool::Surface(f, loc).get()) > 0;
        out_faces_.push_back(f);
        out_template_.emplace_back(-1, Cell{0, 0, 0});
        out_origin_.emplace_back(0, -1);
        out_cavity_.push_back(false);
        out_cap_.push_back(on_body);
      }
    }
    watch.lap("faces");

    std::ostringstream rep;
    rep << "cells kept=" << kept << " clipped=" << cut << " dropped=" << dropped
        << " boolean_warnings=" << warned << " benign_warnings=" << benign_ << " mismatches=" << mismatches_
        << " reparam=" << reparam_ << " twins=" << twins_ << " reconciled=" << reconciled_ << " ";
    report = rep.str();
    if (!reparam_sample_.empty()) {
      report += "| " + reparam_sample_ + " | ";
    }
    if (!warning_sample_.empty() || mismatches_ > 0) {
      report += " | warnings: " + warning_sample_ + " | mismatch cells:" + mismatch_cell_summary() +
                " | mismatches:" + mismatch_log_.str() + " | ";
    }
    if (!benign_sample_.empty()) {
      report += " | benign: " + benign_sample_ + " | ";
    }
    auto result = build_result(report);
    watch.lap("result");
    report += " " + watch.summary();
    return result;
  }

private:
  enum class Class { Keep, Clip, Drop };

  struct Stopwatch {
    std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    std::map<std::string, double> laps;
    void lap(const char *name) {
      const auto now = std::chrono::steady_clock::now();
      laps[name] += std::chrono::duration<double>(now - last).count();
      last = now;
    }
    void reset() { last = std::chrono::steady_clock::now(); }
    std::string summary() const {
      std::ostringstream out;
      out << "time{";
      for (const auto &kv : laps) {
        out << " " << kv.first << "=" << kv.second << "s";
      }
      out << " }";
      return out.str();
    }
  };

  struct CellBoolean {
    std::unique_ptr<BRepAlgoAPI_BooleanOperation> algo;
    bool sound = false; // warned, but checked out (kept without a retry)
    int attempts = 0;   // fuzzy values tried
    int misplaced = 0;  // placement faults of the result (see placement_faults)
    double seconds = 0.0;
  };

  // Vertices that put a clipped cell's result in doubt: result vertices
  // clearly on the side of the body the operation removes, and vertices of
  // the cell solid clearly on the kept side that the result lost (neither
  // themselves nor a copy at the same place). A boolean that kept or dropped
  // a whole region by mistake shows up here even when its result checks out.
  static int placement_faults(const TopoDS_Shape &solid, const TopoDS_Shape &result, const TopoDS_Shape &body,
                              bool keep_outside, double tolerance) {
    BRepClass3d_SolidClassifier classifier(body);
    const TopAbs_State kept = keep_outside ? TopAbs_OUT : TopAbs_IN;
    const TopAbs_State removed = keep_outside ? TopAbs_IN : TopAbs_OUT;
    TopTools_IndexedMapOfShape inputs, outputs;
    TopExp::MapShapes(solid, TopAbs_VERTEX, inputs);
    TopExp::MapShapes(result, TopAbs_VERTEX, outputs);
    int faults = 0;
    std::vector<gp_Pnt> points;
    for (int i = 1; i <= outputs.Extent(); ++i) {
      const TopoDS_Vertex &v = TopoDS::Vertex(outputs(i));
      const gp_Pnt p = BRep_Tool::Pnt(v);
      points.push_back(p);
      classifier.Perform(p, (std::max)(tolerance, BRep_Tool::Tolerance(v)));
      faults += classifier.State() == removed;
    }
    for (int i = 1; i <= inputs.Extent(); ++i) {
      const TopoDS_Vertex &v = TopoDS::Vertex(inputs(i));
      if (outputs.Contains(v)) {
        continue;
      }
      const gp_Pnt p = BRep_Tool::Pnt(v);
      const double tol = (std::max)(tolerance, BRep_Tool::Tolerance(v));
      classifier.Perform(p, tol);
      if (classifier.State() != kept) {
        continue;
      }
      const bool survived = std::any_of(points.begin(), points.end(),
                                        [&](const gp_Pnt &q) { return p.SquareDistance(q) <= tol * tol; });
      faults += !survived;
    }
    return faults;
  }

  // A warned boolean whose result still holds: only alerts about removed
  // slivers, unsplittable edges or orientation guesses, no free edge, and a
  // valid shape. The builder alerts mean faces were lost.
  static bool harmless_result(BRepAlgoAPI_BooleanOperation &algo, const TopoDS_Shape &result) {
    std::ostringstream alerts;
    algo.DumpWarnings(alerts);
    return only_harmless_alerts(alerts.str()) && count_free_edges_of(result) == 0 &&
           BRepCheck_Analyzer(result).IsValid();
  }

  static void report_booleans(const std::vector<CellBoolean> &algos, bool serial,
                              std::chrono::steady_clock::time_point started) {
    double sum = 0.0, longest = 0.0;
    int retried = 0;
    for (const CellBoolean &b : algos) {
      sum += b.seconds;
      longest = (std::max)(longest, b.seconds);
      retried += b.attempts > 1;
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::cerr << "periodic clip: " << algos.size() << " booleans" << (serial ? " (serial)" : "") << " wall " << wall
              << "s cpu " << sum << "s longest " << longest << "s retried " << retried << std::endl;
  }

  struct Pool {
    TopoDS_Shape original;               // kept alive: the map is keyed by its TShape pointer
    std::vector<TopoDS_Shape> pieces;    // first cell's splits (or the original itself)
    std::vector<TopoDS_Shape> generated; // canonical generated sub-shapes
    bool shared = false;                 // seen from a second cell
    bool complete = true;                // every later cell matched the pieces 1:1
    Cell first{0, 0, 0};                 // the cell that made the pool
    bool deleted = false;                // the first cell's boolean deleted the original
  };
  Cell current_cell_{0, 0, 0};
  bool current_deleted_ = false;

  const TopoDS_Shape &cell_;
  double tol_;
  double min_[3], max_[3], period_[3];

  TopTools_IndexedMapOfShape vertices_, edges_, faces_;
  std::vector<Boundary> vertex_info_, edge_info_;
  std::vector<int> cap_side_; // -1 = regular face, else axis*2 + (max ? 1 : 0)
  std::vector<int> face_solid_;   // template solid of each face
  std::vector<bool> face_cavity_; // face lies on a cavity shell of its solid
  // Per output face: (cell, template solid) when instanced from a kept cell
  // (-1 solid otherwise) and whether it came from a cavity shell.
  std::vector<std::pair<int64_t, int>> out_origin_;
  std::vector<std::pair<int, Cell>> out_template_; // (template face, cell) when instanced, else (-1, _)
  std::vector<bool> out_cavity_;
  std::vector<bool> out_cap_; // on the block boundary: a cap, or a piece of the body

  BRep_Builder builder_;
  std::unordered_map<InstanceKey, TopoDS_Vertex, InstanceKeyHash> vertex_instances_;
  std::unordered_map<InstanceKey, TopoDS_Edge, InstanceKeyHash> edge_instances_;
  // Interface faces keyed by (template face on a max side, cell owning that side).
  std::unordered_map<InstanceKey, TopoDS_Face, InstanceKeyHash> face_instances_;
  std::vector<TopoDS_Face> out_faces_;

  std::unordered_map<int64_t, Class> classes_;
  std::unordered_map<const TopoDS_TShape *, Pool> pools_;
  // The shared interface faces split by the body before the cell booleans:
  // each face's pieces, and the substitution of faces / edges / vertices.
  std::unordered_map<const TopoDS_TShape *, std::vector<TopoDS_Shape>> face_splits_;
  BRepTools_ReShape presplit_;
  // Regular faces of the kept cells built before the pre-split.
  std::unordered_map<int64_t, std::vector<TopoDS_Face>> kept_faces_;

  bool touches_clip(const Cell &c) const {
    for (int di = -1; di <= 1; ++di) {
      for (int dj = -1; dj <= 1; ++dj) {
        for (int dk = -1; dk <= 1; ++dk) {
          if (class_of(Cell{c.i + di, c.j + dj, c.k + dk}) == Class::Clip) {
            return true;
          }
        }
      }
    }
    return false;
  }

  // `face` with the pre-split substitutions applied: itself when untouched,
  // else the rebuilt face, or the pieces of a face the body split.
  std::vector<TopoDS_Face> presplit_faces(const TopoDS_Face &face) {
    std::vector<TopoDS_Face> out;
    const TopoDS_Shape applied = presplit_.Apply(face);
    if (applied.ShapeType() == TopAbs_FACE) {
      out.push_back(TopoDS::Face(applied));
    } else {
      for (TopExp_Explorer ex(applied, TopAbs_FACE); ex.More(); ex.Next()) {
        out.push_back(TopoDS::Face(ex.Current()));
      }
    }
    return out;
  }
  std::vector<std::pair<TopoDS_Shape, Bnd_Box>> body_edges_;
  BRepTools_ReShape reshape_;
  std::unordered_set<const TopoDS_TShape *> closed_interfaces_;
  int mismatches_ = 0;
  std::string warning_sample_;
  std::string benign_sample_;
  std::ostringstream mismatch_log_;
  int benign_ = 0;
  int reparam_ = 0;
  int twins_ = 0;      // copies recovered from the results by geometry
  int reconciled_ = 0; // section edges re-split to match the neighbour's
  int warned_logged_ = 0;
  std::string reparam_sample_;

  std::map<int64_t, int> mismatch_cells_;

  std::string mismatch_cell_summary() const {
    std::vector<std::pair<int, int64_t>> order;
    for (const auto &kv : mismatch_cells_) {
      order.emplace_back(kv.second, kv.first);
    }
    std::sort(order.rbegin(), order.rend());
    std::ostringstream out;
    for (size_t i = 0; i < order.size() && i < 6; ++i) {
      const Cell c = unpack_cell(order[i].second);
      out << " (" << c.i << "," << c.j << "," << c.k << ")x" << order[i].first;
    }
    return out.str();
  }

  void log_mismatch(const TopoDS_Shape &original, const char *kind, const std::vector<TopoDS_Shape> &mine,
                    const std::vector<TopoDS_Shape> &pool, const Pool &record) {
    ++mismatches_;
    ++mismatch_cells_[pack_cell(current_cell_.i, current_cell_.j, current_cell_.k)];
    ++mismatch_cells_[pack_cell(record.first.i, record.first.j, record.first.k)];
    if (mismatches_ > 12) {
      return;
    }
    mismatch_log_ << " [" << TopAbs::ShapeTypeToString(original.ShapeType()) << " " << kind << " "
                  << mine.size() << " vs " << pool.size() << " (cell " << current_cell_.i << "," << current_cell_.j
                  << "," << current_cell_.k << " vs " << record.first.i << "," << record.first.j << ","
                  << record.first.k << (record.deleted ? " deleted" : "") << ")";
    auto describe = [&](const TopoDS_Shape &sh) {
      mismatch_log_ << " " << TopAbs::ShapeTypeToString(sh.ShapeType());
      if (sh.ShapeType() == TopAbs_EDGE && !BRep_Tool::Degenerated(TopoDS::Edge(sh))) {
        double f, l;
        Handle(Geom_Curve) c = BRep_Tool::Curve(TopoDS::Edge(sh), f, l);
        const gp_Pnt a = c->Value(f), m = c->Value(0.5 * (f + l)), b = c->Value(l);
        mismatch_log_ << "(" << a.X() << "," << a.Y() << "," << a.Z() << ")-(" << m.X() << "," << m.Y() << ","
                      << m.Z() << ")-(" << b.X() << "," << b.Y() << "," << b.Z() << ")";
      } else if (sh.ShapeType() == TopAbs_VERTEX) {
        const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(sh));
        mismatch_log_ << "(" << p.X() << "," << p.Y() << "," << p.Z() << ")";
      } else if (sh.ShapeType() == TopAbs_FACE) {
        Bnd_Box box;
        BRepBndLib::Add(sh, box, Standard_False);
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        mismatch_log_ << "(" << x0 << "," << y0 << "," << z0 << ")-(" << x1 << "," << y1 << "," << z1 << ") edges{";
        for (TopExp_Explorer ex(sh, TopAbs_EDGE); ex.More(); ex.Next()) {
          const TopoDS_Edge &e = TopoDS::Edge(ex.Current());
          const TopoDS_Shape canon = reshape_.Value(e);
          mismatch_log_ << " " << (canon.IsSame(e) ? "own" : "sub") << ":" << canon.TShape().get();
          if (!BRep_Tool::Degenerated(e)) {
            double f, l;
            Handle(Geom_Curve) c = BRep_Tool::Curve(e, f, l);
            const gp_Pnt m = c->Value(0.5 * (f + l));
            mismatch_log_ << "(" << m.X() << "," << m.Y() << "," << m.Z() << ")";
          }
        }
        mismatch_log_ << "}";
      }
    };
    mismatch_log_ << " mine:";
    for (const TopoDS_Shape &sh : mine) {
      describe(sh);
    }
    mismatch_log_ << " pool:";
    for (const TopoDS_Shape &sh : pool) {
      describe(sh);
    }
    mismatch_log_ << "]";
  }

  // ---- cells -------------------------------------------------------------

  static Cell neighbour(const Cell &c, int side) {
    Cell n = c;
    (side / 2 == 0 ? n.i : side / 2 == 1 ? n.j : n.k) += (side % 2) ? 1 : -1;
    return n;
  }

  Class class_of(const Cell &c) const {
    auto it = classes_.find(pack_cell(c.i, c.j, c.k));
    return it == classes_.end() ? Class::Drop : it->second;
  }

  // Cells the body surface can touch run a boolean; the others are
  // classified by their centre. The surface is located through a coarse
  // triangulation grown by its deflection (conservative).
  void classify_cells(const std::vector<Cell> &cells, const TopoDS_Shape &body, bool keep_outside) {
    const double deflection = 0.02 * (std::min)({period_[0], period_[1], period_[2]});
    BRepMesh_IncrementalMesh mesher(body, deflection, Standard_False, 0.5, Standard_True);
    const double pad = deflection + tol_;

    int lo_cell[3], hi_cell[3];
    for (int a = 0; a < 3; ++a) {
      lo_cell[a] = INT32_MAX;
      hi_cell[a] = INT32_MIN;
    }
    for (const Cell &c : cells) {
      const int idx[3] = {c.i, c.j, c.k};
      for (int a = 0; a < 3; ++a) {
        lo_cell[a] = (std::min)(lo_cell[a], idx[a]);
        hi_cell[a] = (std::max)(hi_cell[a], idx[a]);
      }
    }

    std::unordered_set<int64_t> touched;
    auto mark = [&](const Bnd_Box &box) {
      if (box.IsVoid()) {
        return;
      }
      double lo[3], hi[3];
      box.Get(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
      int lo_i[3], hi_i[3];
      for (int a = 0; a < 3; ++a) {
        const double l = std::floor((lo[a] - pad - min_[a]) / period_[a]);
        const double h = std::floor((hi[a] + pad - min_[a]) / period_[a]);
        lo_i[a] = int((std::max)(l, double(lo_cell[a])));
        hi_i[a] = int((std::min)(h, double(hi_cell[a])));
      }
      for (int i = lo_i[0]; i <= hi_i[0]; ++i) {
        for (int j = lo_i[1]; j <= hi_i[1]; ++j) {
          for (int k = lo_i[2]; k <= hi_i[2]; ++k) {
            const int64_t key = pack_cell(i, j, k);
            if (classes_.count(key)) {
              touched.insert(key);
            }
          }
        }
      }
    };
    for (TopExp_Explorer ex(body, TopAbs_FACE); ex.More(); ex.Next()) {
      const TopoDS_Face &f = TopoDS::Face(ex.Current());
      TopLoc_Location loc;
      Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(f, loc);
      if (tri.IsNull()) {
        Bnd_Box box;
        BRepBndLib::Add(f, box, Standard_False);
        mark(box);
        continue;
      }
      const bool identity = loc.IsIdentity();
      const gp_Trsf &trsf = loc.Transformation();
      for (int t = 1; t <= tri->NbTriangles(); ++t) {
        int n[3];
        tri->Triangle(t).Get(n[0], n[1], n[2]);
        Bnd_Box box;
        for (int v = 0; v < 3; ++v) {
          gp_Pnt p = tri->Node(n[v]);
          if (!identity) {
            p.Transform(trsf);
          }
          box.Add(p);
        }
        mark(box);
      }
    }

    BRepClass3d_SolidClassifier classifier(body);
    for (const Cell &c : cells) {
      const int64_t key = pack_cell(c.i, c.j, c.k);
      if (touched.count(key)) {
        classes_[key] = Class::Clip;
        continue;
      }
      const gp_Pnt centre(min_[0] + (c.i + 0.5) * period_[0], min_[1] + (c.j + 0.5) * period_[1],
                          min_[2] + (c.k + 0.5) * period_[2]);
      classifier.Perform(centre, tol_);
      const TopAbs_State st = classifier.State();
      if (st != TopAbs_IN && st != TopAbs_OUT) {
        classes_[key] = Class::Clip;
        continue;
      }
      classes_[key] = ((st == TopAbs_IN) != keep_outside) ? Class::Keep : Class::Drop;
    }
  }

  void index_body_edges(const TopoDS_Shape &body) {
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(body, TopAbs_EDGE, edges);
    for (int i = 1; i <= edges.Extent(); ++i) {
      Bnd_Box box;
      BRepBndLib::Add(edges(i), box, Standard_False);
      box.Enlarge(tol_);
      body_edges_.emplace_back(edges(i), box);
    }
  }

  // Faces of `cell` on `side` (axis*2 + max). A min side reuses the max-side
  // faces of the neighbour below, reversed, so the two cells meeting at an
  // interface hold the same face instance.
  struct SideFace {
    TopoDS_Face face;
    int index; // template face
    Cell owner;
  };

  std::vector<SideFace> side_faces(const Cell &cell, int side) {
    const int axis = side / 2;
    const bool max_side = (side % 2) == 1;
    const Cell owner = max_side ? cell : neighbour(cell, side);
    const int64_t packed = pack_cell(owner.i, owner.j, owner.k);
    std::vector<SideFace> out;
    for (int fi = 1; fi <= faces_.Extent(); ++fi) {
      if (cap_side_[fi - 1] != axis * 2 + 1) {
        continue;
      }
      const InstanceKey key{fi, packed};
      auto it = face_instances_.find(key);
      if (it == face_instances_.end()) {
        it = face_instances_.emplace(key, instantiate_face(fi, owner)).first;
      }
      out.push_back({max_side ? it->second : TopoDS::Face(it->second.Reversed()), fi, owner});
    }
    return out;
  }

  TopoDS_Shape cell_solid(const Cell &c) {
    std::vector<TopoDS_Face> faces;
    for (int fi = 1; fi <= faces_.Extent(); ++fi) {
      if (cap_side_[fi - 1] < 0) {
        faces.push_back(instantiate_face(fi, c));
      }
    }
    for (int side = 0; side < 6; ++side) {
      for (const SideFace &sf : side_faces(c, side)) {
        faces.push_back(sf.face);
      }
    }
    // Shells: faces connected through shared edges; cavities nested.
    const size_t n = faces.size();
    std::vector<size_t> parent(n);
    for (size_t i = 0; i < n; ++i) {
      parent[i] = i;
    }
    auto find = [&](size_t x) {
      while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
      }
      return x;
    };
    std::unordered_map<const TopoDS_TShape *, size_t> first_face;
    for (size_t i = 0; i < n; ++i) {
      for (TopExp_Explorer ex(faces[i], TopAbs_EDGE); ex.More(); ex.Next()) {
        auto it = first_face.find(ex.Current().TShape().get());
        if (it == first_face.end()) {
          first_face.emplace(ex.Current().TShape().get(), i);
        } else {
          parent[find(it->second)] = find(i);
        }
      }
    }
    std::unordered_map<size_t, TopoDS_Shell> shells;
    std::vector<size_t> order;
    for (size_t i = 0; i < n; ++i) {
      const size_t root = find(i);
      auto it = shells.find(root);
      if (it == shells.end()) {
        TopoDS_Shell sh;
        builder_.MakeShell(sh);
        it = shells.emplace(root, sh).first;
        order.push_back(root);
      }
      builder_.Add(it->second, faces[i]);
    }
    std::vector<TopoDS_Shell> closed;
    for (size_t root : order) {
      shells[root].Closed(Standard_True);
      closed.push_back(shells[root]);
    }
    if (closed.size() == 1) {
      TopoDS_Solid so;
      builder_.MakeSolid(so);
      builder_.Add(so, closed.front());
      return so;
    }
    std::vector<TopoDS_Shape> parts = nest_shells(closed, tol_);
    if (parts.size() == 1) {
      return parts.front();
    }
    TopoDS_Compound comp;
    builder_.MakeCompound(comp);
    for (const TopoDS_Shape &p : parts) {
      builder_.Add(comp, p);
    }
    return comp;
  }

  // ---- clipping ------------------------------------------------------------

  // Boolean of one cell solid with the body, escalating the fuzzy value while
  // the result is unsound. Throws on errors. Safe to call from several threads.
  //
  // A warning on a result that still checks out (no free edge, BRepCheck
  // valid) is a degenerate contact such as a pole of the body touching a cap
  // plane, not lost material; that result is kept as it is, since a larger
  // fuzzy value would only move its vertices away from the neighbours'.
  CellBoolean run_boolean(const TopoDS_Shape &solid, const TopoDS_Shape &body, bool keep_outside) const {
    const auto started = std::chrono::steady_clock::now();
    TopTools_ListOfShape args, tools;
    args.Append(solid);
    tools.Append(body);
    const double diag = std::sqrt(period_[0] * period_[0] + period_[1] * period_[1] + period_[2] * period_[2]);
    // Escalated only while the result is unsound; the last levels approach
    // the template's own tolerances.
    const double fuzzy[5] = {0.0, 1e-6 * diag, 1e-5 * diag, 1e-4 * diag, 1e-3 * diag};
    CellBoolean out;
    for (double f : fuzzy) {
      ++out.attempts;
      if (keep_outside) {
        out.algo.reset(new BRepAlgoAPI_Cut());
      } else {
        out.algo.reset(new BRepAlgoAPI_Common());
      }
      out.algo->SetArguments(args);
      out.algo->SetTools(tools);
      out.algo->SetFuzzyValue(f);
      out.algo->SetRunParallel(Standard_False);
      out.algo->SetNonDestructive(Standard_True);
      out.algo->Build();
      if (out.algo->HasErrors()) {
        std::ostringstream msg;
        out.algo->DumpErrors(msg);
        throw std::runtime_error(msg.str());
      }
      const TopoDS_Shape result = out.algo->Shape();
      out.misplaced = placement_faults(solid, result, body, keep_outside, 1e-3 * diag);
      out.sound = out.misplaced == 0 && (!out.algo->HasWarnings() || harmless_result(*out.algo, result));
      if (out.sound) {
        break;
      }
    }
    out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return out;
  }

  TopoDS_Shape record_cell(const Cell &c, const TopoDS_Shape &solid, CellBoolean &cell_boolean, int &warned) {
    BRepAlgoAPI_BooleanOperation &algo = *cell_boolean.algo;
    const TopoDS_Shape result = algo.Shape();
    if (algo.HasWarnings() || cell_boolean.misplaced > 0) {
      std::ostringstream msg;
      msg << "cell (" << c.i << ", " << c.j << ", " << c.k << ") ";
      if (cell_boolean.misplaced > 0) {
        msg << cell_boolean.misplaced << " vertices misplaced ";
      }
      algo.DumpWarnings(msg);
      std::string text = msg.str();
      std::replace(text.begin(), text.end(), '\n', ' ');
      if (std::getenv("NODI_TPMS_DEBUG") != nullptr && ++warned_logged_ <= 60) {
        std::cerr << "periodic clip: " << text << "(attempts " << cell_boolean.attempts
                  << (cell_boolean.sound ? ", sound)" : ", unsound)") << std::endl;
      }
      if (cell_boolean.sound) {
        ++benign_;
        if (benign_sample_.empty()) {
          benign_sample_ = text;
        }
      } else {
        ++warned;
        if (warning_sample_.empty()) {
          warning_sample_ = text;
        }
      }
    }
    if (TopExp_Explorer(result, TopAbs_FACE).More()) {
      current_cell_ = c;
      record_history(solid, algo, result);
    }
    return result;
  }

  // Register what the boolean made of every original sub-shape of the cell
  // solid (strict: neighbours must agree) and of the body edges crossing it
  // (loose: only the shared crossings coincide).
  void record_history(const TopoDS_Shape &solid, BRepAlgoAPI_BooleanOperation &algo,
                      const TopoDS_Shape &result) {
    TopTools_IndexedMapOfShape in_result;
    TopExp::MapShapes(result, in_result);
    auto images_of = [&](const TopoDS_Shape &s, std::vector<TopoDS_Shape> &pieces) {
      for (const TopoDS_Shape &m : algo.Modified(s)) {
        pieces.push_back(m);
      }
      if (pieces.empty() && in_result.Contains(s)) {
        pieces.push_back(s);
      }
    };

    // Vertices and edges: splits from the history, vertices generated on
    // edges. A copy the boolean made for a tolerance update is missing from
    // the history: an original neither modified nor in the result is looked
    // up in the result by its geometry.
    std::unordered_set<const TopoDS_TShape *> known;
    const TopAbs_ShapeEnum types[2] = {TopAbs_VERTEX, TopAbs_EDGE};
    for (TopAbs_ShapeEnum t : types) {
      TopTools_IndexedMapOfShape originals, candidates;
      TopExp::MapShapes(solid, t, originals);
      TopExp::MapShapes(result, t, candidates);
      std::vector<Bnd_Box> boxes(candidates.Extent());
      for (int i = 1; i <= candidates.Extent(); ++i) {
        BRepBndLib::Add(candidates(i), boxes[i - 1], Standard_False);
      }
      for (int i = 1; i <= originals.Extent(); ++i) {
        const TopoDS_Shape &s = originals(i);
        std::vector<TopoDS_Shape> pieces, generated;
        images_of(s, pieces);
        current_deleted_ = algo.IsDeleted(s);
        if (pieces.empty() && !in_result.Contains(s)) {
          Bnd_Box box;
          BRepBndLib::Add(s, box, Standard_False);
          for (int j = 1; j <= candidates.Extent(); ++j) {
            bool flip = false;
            if (!box.IsOut(boxes[j - 1]) && !known.count(candidates(j).TShape().get()) &&
                same_shape(candidates(j), s, flip)) {
              pieces.push_back(candidates(j));
              ++twins_;
              break;
            }
          }
        }
        if (t == TopAbs_EDGE) {
          for (const TopoDS_Shape &g : algo.Generated(s)) {
            generated.push_back(g);
          }
        }
        known.insert(s.TShape().get());
        for (const TopoDS_Shape &sh : pieces) {
          known.insert(sh.TShape().get());
        }
        for (const TopoDS_Shape &sh : generated) {
          known.insert(sh.TShape().get());
        }
        unify(s, pieces, generated, true);
      }
    }

    // Faces: their splits, and as generated shapes the section edges bounding
    // those splits (and the section vertices on them) that are no image of an
    // original vertex / edge. The history does not list them for faces.
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(solid, TopAbs_FACE, faces);
    for (int i = 1; i <= faces.Extent(); ++i) {
      const TopoDS_Shape &f = faces(i);
      std::vector<TopoDS_Shape> pieces, generated;
      images_of(f, pieces);
      std::unordered_set<const TopoDS_TShape *> seen;
      for (const TopoDS_Shape &g : algo.Generated(f)) {
        if (seen.insert(g.TShape().get()).second) {
          generated.push_back(g);
        }
      }
      for (const TopoDS_Shape &piece : pieces) {
        for (TopExp_Explorer ex(piece, TopAbs_EDGE); ex.More(); ex.Next()) {
          const TopoDS_Shape &e = ex.Current();
          if (known.count(e.TShape().get()) || !seen.insert(e.TShape().get()).second) {
            continue;
          }
          generated.push_back(e);
          for (TopExp_Explorer vx(e, TopAbs_VERTEX); vx.More(); vx.Next()) {
            const TopoDS_Shape &v = vx.Current();
            if (!known.count(v.TShape().get()) && seen.insert(v.TShape().get()).second) {
              generated.push_back(v);
            }
          }
        }
      }
      unify(f, pieces, generated, true);
    }

    Bnd_Box cell_box;
    BRepBndLib::Add(solid, cell_box, Standard_False);
    for (const auto &be : body_edges_) {
      if (cell_box.IsOut(be.second)) {
        continue;
      }
      std::vector<TopoDS_Shape> none, generated;
      for (const TopoDS_Shape &g : algo.Generated(be.first)) {
        generated.push_back(g);
      }
      if (!generated.empty()) {
        unify(be.first, none, generated, false);
      }
    }
  }

  void unify(const TopoDS_Shape &original, const std::vector<TopoDS_Shape> &pieces,
             const std::vector<TopoDS_Shape> &generated, bool strict) {
    const TopoDS_TShape *key = original.TShape().get();
    auto it = pools_.find(key);
    if (it == pools_.end()) {
      Pool p;
      p.original = original;
      p.pieces = pieces;
      p.generated = generated;
      p.first = current_cell_;
      p.deleted = current_deleted_;
      pools_.emplace(key, std::move(p));
      return;
    }
    Pool &pool = it->second;
    pool.shared = true;

    for (const TopoDS_Shape &g : generated) {
      bool flip = false;
      const TopoDS_Shape *match = find_match(pool.generated, g, flip);
      if (match) {
        substitute(g, *match, flip);
      } else if (strict && g.ShapeType() == TopAbs_EDGE && reconcile_split(pool, TopoDS::Edge(g), generated)) {
        ++reconciled_;
      } else {
        if (strict) {
          pool.complete = false;
          log_mismatch(original, "generated", generated, pool.generated, pool);
        }
        pool.generated.push_back(g);
      }
    }
    if (!strict) {
      return;
    }

    std::vector<std::pair<const TopoDS_Shape *, bool>> matched(pieces.size(), {nullptr, false});
    bool complete = pieces.size() == pool.pieces.size();
    for (size_t i = 0; i < pieces.size(); ++i) {
      bool flip = false;
      matched[i].first = find_match(pool.pieces, pieces[i], flip);
      matched[i].second = flip;
      complete &= matched[i].first != nullptr;
    }
    if (!complete) {
      pool.complete = false;
      // A vertex a boolean merged away in one cell leaves the edges to the
      // geometric matching; the closure checks catch any real gap.
      if (original.ShapeType() == TopAbs_VERTEX && (pieces.empty() || pool.pieces.empty())) {
        ++benign_;
      } else {
        log_mismatch(original, "pieces", pieces, pool.pieces, pool);
      }
    }
    // Face pieces are swapped only as a whole: a partial swap would leave the
    // same face in two shells.
    if (original.ShapeType() == TopAbs_FACE && !complete) {
      return;
    }
    for (size_t i = 0; i < pieces.size(); ++i) {
      if (matched[i].first) {
        substitute(pieces[i], *matched[i].first, matched[i].second);
      }
    }
  }

  void substitute(const TopoDS_Shape &from, const TopoDS_Shape &to, bool flip) {
    if (from.IsSame(to)) {
      return;
    }
    reshape_.Replace(from.Oriented(TopAbs_FORWARD),
                     to.Oriented(flip ? TopAbs_REVERSED : TopAbs_FORWARD));
  }

  const TopoDS_Shape *find_match(const std::vector<TopoDS_Shape> &candidates, const TopoDS_Shape &s,
                                 bool &flip) const {
    for (const TopoDS_Shape &c : candidates) {
      if (same_shape(c, s, flip)) {
        return &c;
      }
    }
    return nullptr;
  }

  // Geometric identity of two sub-shapes made independently from the same
  // originals: vertices by position, edges by three interior samples (either
  // direction), faces by their canonical edge set.
  bool same_shape(const TopoDS_Shape &a, const TopoDS_Shape &b, bool &flip) const {
    flip = false;
    if (a.ShapeType() != b.ShapeType()) {
      return false;
    }
    switch (a.ShapeType()) {
    case TopAbs_VERTEX: {
      const TopoDS_Vertex &va = TopoDS::Vertex(a);
      const TopoDS_Vertex &vb = TopoDS::Vertex(b);
      const double tol = (std::max)({tol_, BRep_Tool::Tolerance(va), BRep_Tool::Tolerance(vb)});
      return within(BRep_Tool::Pnt(va), BRep_Tool::Pnt(vb), tol);
    }
    case TopAbs_EDGE:
      return same_edge(TopoDS::Edge(a), TopoDS::Edge(b), flip);
    case TopAbs_FACE:
      return canonical_edges(a) == canonical_edges(b);
    default:
      return false;
    }
  }

  // Same end vertices (either order) and three interior points of `a` lying
  // on `b`: independent of how each boolean parametrised the curve.
  bool same_edge(const TopoDS_Edge &a, const TopoDS_Edge &b, bool &flip) const {
    if (BRep_Tool::Degenerated(a) || BRep_Tool::Degenerated(b)) {
      return false;
    }
    const double tol = (std::max)({tol_, BRep_Tool::Tolerance(a), BRep_Tool::Tolerance(b)});
    TopoDS_Vertex a0, a1, b0, b1;
    TopExp::Vertices(a, a0, a1);
    TopExp::Vertices(b, b0, b1);
    if (a0.IsNull() || a1.IsNull() || b0.IsNull() || b1.IsNull()) {
      return false;
    }
    const gp_Pnt pa0 = BRep_Tool::Pnt(a0), pa1 = BRep_Tool::Pnt(a1);
    const gp_Pnt pb0 = BRep_Tool::Pnt(b0), pb1 = BRep_Tool::Pnt(b1);
    // Ends made by independent booleans sit anywhere within their vertex
    // tolerances.
    auto ends_meet = [&](const TopoDS_Vertex &va, const gp_Pnt &pa, const TopoDS_Vertex &vb, const gp_Pnt &pb) {
      return within(pa, pb, (std::max)(tol, BRep_Tool::Tolerance(va) + BRep_Tool::Tolerance(vb)));
    };
    const bool closed = within(pa0, pa1, tol);
    if (closed) {
      if (!within(pb0, pb1, tol) || !ends_meet(a0, pa0, b0, pb0)) {
        return false;
      }
    } else if (ends_meet(a0, pa0, b0, pb0) && ends_meet(a1, pa1, b1, pb1)) {
      flip = false;
    } else if (ends_meet(a0, pa0, b1, pb1) && ends_meet(a1, pa1, b0, pb0)) {
      flip = true;
    } else {
      if (std::getenv("NODI_TPMS_DEBUG") != nullptr &&
          (within(pa0, pb0, 10 * tol) && within(pa1, pb1, 10 * tol) ||
           within(pa0, pb1, 10 * tol) && within(pa1, pb0, 10 * tol))) {
        std::cerr << "same_edge: ends miss by " << (std::min)(pa0.Distance(pb0), pa0.Distance(pb1)) << " / "
                  << (std::min)(pa1.Distance(pb1), pa1.Distance(pb0)) << " at tol " << tol << " (edge tols "
                  << BRep_Tool::Tolerance(a) << ", " << BRep_Tool::Tolerance(b) << "; vertex tols "
                  << BRep_Tool::Tolerance(a0) << ", " << BRep_Tool::Tolerance(a1) << ", " << BRep_Tool::Tolerance(b0)
                  << ", " << BRep_Tool::Tolerance(b1) << ") near (" << pa0.X() << ", " << pa0.Y() << ", " << pa0.Z()
                  << ")" << std::endl;
      }
      return false;
    }
    double fb, lb;
    Handle(Geom_Curve) cb = BRep_Tool::Curve(b, fb, lb);
    gp_Pnt pa[3];
    sample_interior(a, pa);
    double t_mid = 0.0;
    for (int i = 0; i < 3; ++i) {
      GeomAPI_ProjectPointOnCurve proj(pa[i], cb, fb, lb);
      if (proj.NbPoints() == 0 || proj.LowerDistance() > tol) {
        if (std::getenv("NODI_TPMS_DEBUG") != nullptr) {
          std::cerr << "same_edge: interior sample " << i << " misses by "
                    << (proj.NbPoints() == 0 ? -1.0 : proj.LowerDistance()) << " at tol " << tol << " near ("
                    << pa[i].X() << ", " << pa[i].Y() << ", " << pa[i].Z() << ")" << std::endl;
        }
        return false;
      }
      if (i == 1) {
        t_mid = proj.LowerDistanceParameter();
      }
    }
    if (closed) {
      double fa, la;
      Handle(Geom_Curve) ca = BRep_Tool::Curve(a, fa, la);
      gp_Pnt p;
      gp_Vec ta, tb;
      ca->D1(0.5 * (fa + la), p, ta);
      cb->D1(t_mid, p, tb);
      flip = ta.Dot(tb) < 0.0;
    }
    return true;
  }

  static bool only_harmless_alerts(const std::string &dump) {
    std::istringstream lines(dump);
    std::string line;
    while (std::getline(lines, line)) {
      while (!line.empty() && line.back() <= ' ') {
        line.pop_back();
      }
      if (line.empty()) {
        continue;
      }
      // An edge the boolean could not split stays whole in a result that
      // still closes; a fuzzy retry does not change that.
      if (line != "BOPAlgo_AlertTooSmallEdge" && line != "BOPAlgo_AlertUnableToOrientTheShape" &&
          line != "BOPAlgo_AlertNotSplittableEdge") {
        return false;
      }
    }
    return true;
  }

  // `piece`, a split of `edge`, oriented to run the way `edge` runs forward.
  static TopoDS_Shape oriented_like(const TopoDS_Edge &edge, const TopoDS_Edge &piece) {
    double f, l, pf, pl;
    Handle(Geom_Curve) c = BRep_Tool::Curve(edge, f, l);
    Handle(Geom_Curve) pc = BRep_Tool::Curve(piece, pf, pl);
    if (c.IsNull() || pc.IsNull()) {
      return piece.Oriented(TopAbs_FORWARD);
    }
    gp_Pnt p;
    gp_Vec tangent, piece_tangent;
    pc->D1(0.5 * (pf + pl), p, piece_tangent);
    GeomAPI_ProjectPointOnCurve projection(p, c, f, l);
    if (projection.NbPoints() == 0) {
      return piece.Oriented(TopAbs_FORWARD);
    }
    c->D1(projection.LowerDistanceParameter(), p, tangent);
    return piece.Oriented(tangent.Dot(piece_tangent) < 0.0 ? TopAbs_REVERSED : TopAbs_FORWARD);
  }

  static void sample_interior(const TopoDS_Edge &e, gp_Pnt out[3]) {
    double f, l;
    Handle(Geom_Curve) c = BRep_Tool::Curve(e, f, l);
    if (c.IsNull()) {
      throw std::runtime_error("periodic cell: clipped edge without 3D curve");
    }
    const double d = l - f;
    out[0] = c->Value(f + 0.25 * d);
    out[1] = c->Value(f + 0.5 * d);
    out[2] = c->Value(f + 0.75 * d);
  }

  std::vector<const TopoDS_TShape *> canonical_edges(const TopoDS_Shape &face) const {
    std::vector<const TopoDS_TShape *> ids;
    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
      const TopoDS_Shape value = reshape_.Value(ex.Current());
      if (value.ShapeType() == TopAbs_EDGE) {
        ids.push_back(value.TShape().get());
      } else {
        // An edge substituted by its split.
        for (TopExp_Explorer sx(value, TopAbs_EDGE); sx.More(); sx.Next()) {
          ids.push_back(sx.Current().TShape().get());
        }
      }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
  }

  // Whether `part` runs along `whole` within its range: both ends and three
  // interior points of `part` lie on the curve of `whole`.
  bool covered_by(const TopoDS_Edge &part, const TopoDS_Edge &whole, double &from, double &to) const {
    if (BRep_Tool::Degenerated(part) || BRep_Tool::Degenerated(whole)) {
      return false;
    }
    const double tol = (std::max)({tol_, BRep_Tool::Tolerance(part), BRep_Tool::Tolerance(whole)});
    double f, l, pf, pl;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(whole, f, l);
    Handle(Geom_Curve) pc = BRep_Tool::Curve(part, pf, pl);
    if (curve.IsNull() || pc.IsNull()) {
      return false;
    }
    const double slack = 0.5 * (l - f);
    double params[5];
    for (int i = 0; i < 5; ++i) {
      const gp_Pnt q = pc->Value(pf + (pl - pf) * i / 4.0);
      GeomAPI_ProjectPointOnCurve projection(q, curve, f - slack, l + slack);
      const double vertex_tol = i == 0 || i == 4 ? 4.0 * tol : tol;
      if (projection.NbPoints() == 0 || projection.LowerDistance() > vertex_tol) {
        return false;
      }
      params[i] = projection.LowerDistanceParameter();
    }
    from = (std::min)(params[0], params[4]);
    to = (std::max)(params[0], params[4]);
    // Inside the range, up to a vertex tolerance in parameter.
    gp_Pnt a, b;
    gp_Vec d;
    curve->D1(f, a, d);
    const double eps = 4.0 * tol / (std::max)(d.Magnitude(), 1e-12);
    return from >= f - eps && to <= l + eps;
  }

  // The edges of `parts` that together run along `whole` end to end, in
  // order and each oriented like `whole`; empty when they do not cover it.
  std::vector<TopoDS_Shape> chain_covering(const TopoDS_Edge &whole, const std::vector<TopoDS_Shape> &parts) const {
    std::vector<std::pair<double, TopoDS_Shape>> covering;
    double f, l;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(whole, f, l);
    if (curve.IsNull()) {
      return {};
    }
    std::vector<std::pair<double, double>> ranges;
    for (const TopoDS_Shape &p : parts) {
      if (p.ShapeType() != TopAbs_EDGE) {
        continue;
      }
      double from, to;
      if (covered_by(TopoDS::Edge(p), whole, from, to)) {
        covering.emplace_back(from, oriented_like(whole, TopoDS::Edge(p)));
        ranges.emplace_back(from, to);
      }
    }
    if (covering.size() < 2) {
      return {};
    }
    std::vector<size_t> order(covering.size());
    for (size_t i = 0; i < order.size(); ++i) {
      order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return covering[a].first < covering[b].first; });
    gp_Pnt a;
    gp_Vec d;
    curve->D1(f, a, d);
    const double eps = 4.0 * (std::max)(tol_, BRep_Tool::Tolerance(whole)) / (std::max)(d.Magnitude(), 1e-12);
    double reached = f;
    for (size_t i : order) {
      if (std::fabs(ranges[i].first - reached) > eps) {
        return {};
      }
      reached = ranges[i].second;
    }
    if (std::fabs(reached - l) > eps) {
      return {};
    }
    std::vector<TopoDS_Shape> out;
    for (size_t i : order) {
      out.push_back(covering[i].second);
    }
    return out;
  }

  // A section edge the two cells split differently: when one side's edge is
  // the other side's edges end to end, the coarser one is replaced by the
  // finer ones, so both cells end up with the same edges.
  bool reconcile_split(Pool &pool, const TopoDS_Edge &mine, const std::vector<TopoDS_Shape> &generated) {
    // Mine is coarser: the pool's edges cover it.
    std::vector<TopoDS_Shape> finer = chain_covering(mine, pool.generated);
    if (!finer.empty()) {
      TopoDS_Compound compound;
      builder_.MakeCompound(compound);
      for (const TopoDS_Shape &e : finer) {
        builder_.Add(compound, e);
      }
      reshape_.Replace(mine.Oriented(TopAbs_FORWARD), compound);
      return true;
    }
    // Mine is finer: it and its siblings cover one of the pool's edges.
    for (size_t i = 0; i < pool.generated.size(); ++i) {
      if (pool.generated[i].ShapeType() != TopAbs_EDGE) {
        continue;
      }
      const TopoDS_Edge coarse = TopoDS::Edge(pool.generated[i]);
      double from, to;
      if (!covered_by(mine, coarse, from, to)) {
        continue;
      }
      std::vector<TopoDS_Shape> cover = chain_covering(coarse, generated);
      if (cover.empty()) {
        return false;
      }
      TopoDS_Compound compound;
      builder_.MakeCompound(compound);
      for (const TopoDS_Shape &e : cover) {
        builder_.Add(compound, e);
      }
      reshape_.Replace(coarse.Oriented(TopAbs_FORWARD), compound);
      // The finer edges become the canonical ones.
      pool.generated.erase(pool.generated.begin() + i);
      for (const TopoDS_Shape &e : cover) {
        pool.generated.push_back(e.Oriented(TopAbs_FORWARD));
      }
      return true;
    }
    return false;
  }

  // Split the interface faces instantiated so far (those of the clipped
  // cells) by the body in one operation. Two booleans run independently
  // split a shared face differently where the body grazes a vertex or an
  // edge within its tolerance; started from one split, both keep its pieces.
  void presplit_interfaces(const TopoDS_Shape &body) {
    TopTools_ListOfShape args, tools;
    TopoDS_Compound skeleton;
    builder_.MakeCompound(skeleton);
    std::unordered_set<const TopoDS_TShape *> seen;
    for (const auto &kv : face_instances_) {
      if (seen.insert(kv.second.TShape().get()).second) {
        args.Append(kv.second);
        builder_.Add(skeleton, kv.second);
      }
    }
    if (args.IsEmpty()) {
      return;
    }
    tools.Append(body);
    const auto started = std::chrono::steady_clock::now();
    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(args);
    splitter.SetTools(tools);
    // Destructive on purpose: tolerance updates land on the shared sub-shapes
    // themselves instead of copies the kept cells would not see.
    splitter.SetNonDestructive(Standard_False);
    splitter.SetRunParallel(Standard_True);
    splitter.Build();
    if (splitter.HasErrors()) {
      if (std::getenv("NODI_TPMS_DEBUG") != nullptr) {
        std::ostringstream msg;
        splitter.DumpErrors(msg);
        std::cerr << "periodic clip: interface pre-split failed, cells split independently: " << msg.str()
                  << std::endl;
      }
      return;
    }
    // Only the edges and vertices take the split: a face handed to a boolean
    // with edges already lying on the body makes the boolean lose faces
    // (unused edges in its face builder). Each cell splits the interface
    // face itself, from the shared split edges, so the sections agree.
    int split_edges = 0;
    const TopAbs_ShapeEnum types[2] = {TopAbs_EDGE, TopAbs_VERTEX};
    for (TopAbs_ShapeEnum t : types) {
      TopTools_IndexedMapOfShape originals;
      TopExp::MapShapes(skeleton, t, originals);
      for (int i = 1; i <= originals.Extent(); ++i) {
        const TopoDS_Shape &s = originals(i);
        const TopTools_ListOfShape &modified = splitter.Modified(s);
        if (modified.IsEmpty()) {
          continue;
        }
        TopoDS_Compound pieces;
        builder_.MakeCompound(pieces);
        for (const TopoDS_Shape &m : modified) {
          // Split edges come in either direction; a piece must stand in for
          // the original taken forward.
          builder_.Add(pieces, t == TopAbs_EDGE ? oriented_like(TopoDS::Edge(s), TopoDS::Edge(m))
                                                : m.Oriented(TopAbs_FORWARD));
        }
        presplit_.Replace(s.Oriented(TopAbs_FORWARD), pieces);
        split_edges += t == TopAbs_EDGE;
      }
    }
    // The interface faces rebuilt on the split edges, once, so that both
    // cells of an interface hold the same face.
    int rebuilt = 0;
    for (const auto &kv : face_instances_) {
      const TopoDS_Face &f = kv.second;
      if (face_splits_.count(f.TShape().get())) {
        continue;
      }
      const TopoDS_Shape applied = presplit_.Apply(f.Oriented(TopAbs_FORWARD));
      if (applied.IsSame(f)) {
        continue;
      }
      presplit_.Replace(f.Oriented(TopAbs_FORWARD), applied.Oriented(TopAbs_FORWARD));
      face_splits_.emplace(f.TShape().get(), std::vector<TopoDS_Shape>{applied.Oriented(TopAbs_FORWARD)});
      ++rebuilt;
    }
    if (std::getenv("NODI_TPMS_DEBUG") != nullptr) {
      const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
      std::cerr << "periodic clip: pre-split " << args.Extent() << " interface faces in " << seconds << "s: "
                << split_edges << " edges split, " << rebuilt << " faces rebuilt";
      if (splitter.HasWarnings()) {
        std::cerr << "; warnings: ";
        splitter.DumpWarnings(std::cerr);
      }
      std::cerr << std::endl;
    }
  }

  // Interface faces whose pieces both cells agree on are internal: drop them
  // from the clipped results. Disagreements leave both sides' pieces as caps.
  void resolve_interfaces() {
    for (const auto &kv : face_instances_) {
      const TopoDS_Face &f = kv.second;
      const int fi = kv.first.canon;
      const int axis = cap_side_[fi - 1] / 2;
      const Cell owner = unpack_cell(kv.first.cell);
      const Class a = class_of(owner);
      const Class b = class_of(neighbour(owner, axis * 2 + 1));
      // The face itself, or the pieces the pre-split made of it.
      std::vector<TopoDS_Shape> parts;
      auto split = face_splits_.find(f.TShape().get());
      if (split == face_splits_.end()) {
        parts.push_back(f);
      } else {
        parts = split->second;
      }
      bool internal = true;
      std::vector<const Pool *> pools;
      for (const TopoDS_Shape &part : parts) {
        auto pit = pools_.find(part.TShape().get());
        if (pit == pools_.end()) {
          internal = false;
          break;
        }
        const Pool &pool = pit->second;
        if (a == Class::Clip && b == Class::Clip) {
          internal &= pool.shared && pool.complete;
        } else if (a == Class::Keep || b == Class::Keep) {
          internal &= pool.pieces.size() == 1 && pool.pieces.front().IsSame(part);
        } else {
          internal = false;
        }
        pools.push_back(&pool);
      }
      if (!internal) {
        continue;
      }
      for (const Pool *pool : pools) {
        for (const TopoDS_Shape &p : pool->pieces) {
          reshape_.Remove(p);
        }
      }
      closed_interfaces_.insert(f.TShape().get());
    }
  }

  bool interface_closed(const TopoDS_Face &f) const { return closed_interfaces_.count(f.TShape().get()) > 0; }

  static Cell unpack_cell(int64_t packed) {
    const int64_t off = int64_t(1) << 20;
    const int64_t mask = (int64_t(1) << 21) - 1;
    return Cell{int(((packed >> 42) & mask) - off), int(((packed >> 21) & mask) - off),
                int((packed & mask) - off)};
  }

  // ---- geometry helpers --------------------------------------------------

  gp_Vec translation(const Cell &c) const {
    return gp_Vec(c.i * period_[0], c.j * period_[1], c.k * period_[2]);
  }

  gp_Trsf trsf(const Cell &c) const {
    gp_Trsf t;
    t.SetTranslation(translation(c));
    return t;
  }

  static Cell shifted(const Cell &c, const int shift[3]) {
    return Cell{c.i - shift[0], c.j - shift[1], c.k - shift[2]};
  }

  bool on_min(const gp_Pnt &p, int a) const { return std::fabs(p.Coord(a + 1) - min_[a]) <= tol_; }

  bool within(const gp_Pnt &a, const gp_Pnt &b, double tol) const { return a.Distance(b) <= tol; }

  gp_Pnt translated(const gp_Pnt &p, const int shift[3]) const {
    return gp_Pnt(p.X() + shift[0] * period_[0], p.Y() + shift[1] * period_[1],
                  p.Z() + shift[2] * period_[2]);
  }

  // Five samples along an edge: ends, quarter, middle, three quarters.
  // Whether every point lies on the edge's curve within `tol`.
  static bool lies_on(const gp_Pnt *points, int count, const TopoDS_Edge &e, double tol) {
    double f, l;
    Handle(Geom_Curve) c = BRep_Tool::Curve(e, f, l);
    if (c.IsNull()) {
      return false;
    }
    for (int i = 0; i < count; ++i) {
      GeomAPI_ProjectPointOnCurve proj(points[i], c, f, l);
      if (proj.NbPoints() == 0 || proj.LowerDistance() > tol) {
        return false;
      }
    }
    return true;
  }

  static void sample_edge(const TopoDS_Edge &e, gp_Pnt out[5]) {
    double f, l;
    Handle(Geom_Curve) c = BRep_Tool::Curve(e, f, l);
    if (c.IsNull()) {
      throw std::runtime_error("periodic cell: edge without 3D curve");
    }
    const double d = l - f;
    const double params[5] = {f, l, f + 0.25 * d, f + 0.5 * d, f + 0.75 * d};
    for (int i = 0; i < 5; ++i) {
      out[i] = c->Value(params[i]);
    }
  }

  // ---- template classification ------------------------------------------

  void classify_vertices() {
    TopExp::MapShapes(cell_, TopAbs_VERTEX, vertices_);
    const int n = vertices_.Extent();
    vertex_info_.assign(n, Boundary{});
    std::vector<gp_Pnt> pts(n);
    for (int i = 1; i <= n; ++i) {
      pts[i - 1] = BRep_Tool::Pnt(TopoDS::Vertex(vertices_(i)));
    }
    for (int i = 0; i < n; ++i) {
      Boundary &b = vertex_info_[i];
      b.canon = i;
      bool boundary = false;
      for (int a = 0; a < 3; ++a) {
        b.shift[a] = on_min(pts[i], a) ? 1 : 0;
        boundary |= b.shift[a] != 0;
      }
      if (!boundary) {
        continue;
      }
      const gp_Pnt target = translated(pts[i], b.shift);
      const double tv = BRep_Tool::Tolerance(TopoDS::Vertex(vertices_(i + 1)));
      int found = -1;
      for (int j = 0; j < n && found < 0; ++j) {
        if (j == i) {
          continue;
        }
        const double tj = BRep_Tool::Tolerance(TopoDS::Vertex(vertices_(j + 1)));
        if (within(pts[j], target, (std::max)(tol_, tv + tj))) {
          found = j;
        }
      }
      if (found < 0) {
        std::ostringstream msg;
        msg << "periodic cell: vertex (" << pts[i].X() << ", " << pts[i].Y() << ", " << pts[i].Z()
            << ") has no twin at (" << target.X() << ", " << target.Y() << ", " << target.Z() << ")";
        throw std::runtime_error(msg.str());
      }
      b.canon = found;
    }
    // A twin must itself be canonical (lie on no min face); chase if not.
    for (int i = 0; i < n; ++i) {
      int c = vertex_info_[i].canon;
      int guard = 0;
      while (vertex_info_[c].canon != c && guard++ < 8) {
        c = vertex_info_[c].canon;
      }
      if (vertex_info_[c].canon != c) {
        throw std::runtime_error("periodic cell: vertex twin chain does not terminate");
      }
      vertex_info_[i].canon = c;
      // Accumulate the shift along the chain: the twin's own shift applies too.
      // (Only happens when a vertex lies on a min face of one axis and its
      // twin on a min face of another; shifts are then combined.)
    }
    recompute_vertex_shifts(pts);
  }

  // After chasing twin chains, derive each vertex's shift from positions so
  // that pts[i] + shift * period == pts[canon].
  void recompute_vertex_shifts(const std::vector<gp_Pnt> &pts) {
    for (size_t i = 0; i < pts.size(); ++i) {
      Boundary &b = vertex_info_[i];
      const gp_Pnt &c = pts[b.canon];
      for (int a = 0; a < 3; ++a) {
        const double s = (c.Coord(a + 1) - pts[i].Coord(a + 1)) / period_[a];
        const int si = int(std::lround(s));
        if (std::fabs(s - si) > 1e-6 || si < 0 || si > 1) {
          std::ostringstream msg;
          msg << "periodic cell: vertex twin is not one period away: (" << pts[i].X() << ", " << pts[i].Y() << ", "
              << pts[i].Z() << ") -> (" << c.X() << ", " << c.Y() << ", " << c.Z() << ")";
          throw std::runtime_error(msg.str());
        }
        b.shift[a] = si;
      }
    }
  }

  void classify_edges() {
    TopExp::MapShapes(cell_, TopAbs_EDGE, edges_);
    const int n = edges_.Extent();
    edge_info_.assign(n, Boundary{});
    std::vector<std::array<gp_Pnt, 5>> samples(n);
    std::vector<double> range_len(n, 0.0);
    for (int i = 0; i < n; ++i) {
      const TopoDS_Edge e = TopoDS::Edge(edges_(i + 1));
      Boundary &b = edge_info_[i];
      b.canon = i;
      if (BRep_Tool::Degenerated(e)) {
        b.degenerated = true;
        continue;
      }
      sample_edge(e, samples[i].data());
      double f, l;
      BRep_Tool::Range(e, f, l);
      range_len[i] = l - f;
    }
    for (int i = 0; i < n; ++i) {
      Boundary &b = edge_info_[i];
      if (b.degenerated) {
        continue;
      }
      bool boundary = false;
      for (int a = 0; a < 3; ++a) {
        bool all = true;
        for (int s = 0; s < 5; ++s) {
          all &= on_min(samples[i][s], a);
        }
        b.shift[a] = all ? 1 : 0;
        boundary |= all;
      }
      if (!boundary) {
        continue;
      }
      gp_Pnt target[5];
      for (int s = 0; s < 5; ++s) {
        target[s] = translated(samples[i][s], b.shift);
      }
      const double te = BRep_Tool::Tolerance(TopoDS::Edge(edges_(i + 1)));
      int found = -1;
      bool reversed = false;
      bool reparametrised = false;
      for (int j = 0; j < n && found < 0; ++j) {
        if (j == i || edge_info_[j].degenerated) {
          continue;
        }
        const double m = (std::max)(tol_, te + BRep_Tool::Tolerance(TopoDS::Edge(edges_(j + 1))));
        const auto &sj = samples[j];
        const bool forward_ends = within(sj[0], target[0], m) && within(sj[1], target[1], m);
        const bool reversed_ends = within(sj[0], target[1], m) && within(sj[1], target[0], m);
        if (!forward_ends && !reversed_ends) {
          continue;
        }
        if (within(sj[3], target[3], m) && forward_ends && within(sj[2], target[2], m) &&
            within(sj[4], target[4], m)) {
          found = j;
          reversed = false;
        } else if (within(sj[3], target[3], m) && reversed_ends && within(sj[2], target[4], m) &&
                   within(sj[4], target[2], m)) {
          found = j;
          reversed = true;
        } else {
          // Same ends, interior samples elsewhere: the same curve parametrised
          // differently if each edge's samples lie on the other's curve.
          gp_Pnt back[3];
          for (int s = 0; s < 3; ++s) {
            const gp_Pnt &p = sj[s + 2];
            back[s] = gp_Pnt(p.X() - b.shift[0] * period_[0], p.Y() - b.shift[1] * period_[1],
                             p.Z() - b.shift[2] * period_[2]);
          }
          if (lies_on(target + 2, 3, TopoDS::Edge(edges_(j + 1)), m) &&
              lies_on(back, 3, TopoDS::Edge(edges_(i + 1)), m)) {
            found = j;
            reversed = !forward_ends;
            reparametrised = true;
          }
        }
      }
      if (found < 0) {
        const gp_Pnt &p = samples[i][3];
        std::ostringstream msg;
        msg << "periodic cell: edge through (" << p.X() << ", " << p.Y() << ", " << p.Z()
            << ") has no twin on the opposite face";
        throw std::runtime_error(msg.str());
      }
      // Section curves approximated independently on the two faces share
      // geometry but not parametrisation: their pcurves are re-projected.
      b.project = reparametrised ||
                  std::fabs(range_len[found] - range_len[i]) > 1e-7 * (std::max)(1.0, range_len[i]);
      if (b.project) {
        ++reparam_;
        if (reparam_sample_.empty()) {
          const gp_Pnt &p = samples[i][3];
          std::ostringstream msg;
          msg << (reparametrised ? "reparametrised" : "range") << " twin through (" << p.X() << ", "
              << p.Y() << ", " << p.Z() << ")";
          reparam_sample_ = msg.str();
        }
      }
      b.canon = found;
      b.reversed = reversed;
    }
    // Chase twin chains (an edge on two min faces may map to an edge on one
    // min face first) and compose reversal / parameter offsets.
    for (int i = 0; i < n; ++i) {
      Boundary &b = edge_info_[i];
      int c = b.canon;
      bool rev = b.reversed;
      int guard = 0;
      while (edge_info_[c].canon != c && guard++ < 8) {
        rev ^= edge_info_[c].reversed;
        b.project |= edge_info_[c].project;
        c = edge_info_[c].canon;
      }
      if (edge_info_[c].canon != c) {
        throw std::runtime_error("periodic cell: edge twin chain does not terminate");
      }
      b.canon = c;
      b.reversed = rev;
      // Shift from the positions of the start vertices.
      if (!b.degenerated) {
        const gp_Pnt &mine = samples[i][0];
        const gp_Pnt &theirs = samples[c][rev ? 1 : 0];
        for (int a = 0; a < 3; ++a) {
          const double s = (theirs.Coord(a + 1) - mine.Coord(a + 1)) / period_[a];
          const int si = int(std::lround(s));
          if (std::fabs(s - si) > 1e-6 || si < 0 || si > 1) {
            throw std::runtime_error("periodic cell: edge twin is not one period away");
          }
          b.shift[a] = si;
        }
        double f, l, fc, lc;
        BRep_Tool::Range(TopoDS::Edge(edges_(i + 1)), f, l);
        BRep_Tool::Range(TopoDS::Edge(edges_(c + 1)), fc, lc);
        b.delta = fc - f;
      }
    }
  }

  void classify_faces() {
    for (TopExp_Explorer ex(cell_, TopAbs_FACE); ex.More(); ex.Next()) {
      faces_.Add(ex.Current());
    }
    const int n = faces_.Extent();
    cap_side_.assign(n, -1);
    // Which template solid each face bounds, and whether from a cavity shell.
    face_solid_.assign(n, -1);
    face_cavity_.assign(n, false);
    int solid_index = 0;
    for (TopExp_Explorer sx(cell_, TopAbs_SOLID); sx.More(); sx.Next(), ++solid_index) {
      const TopoDS_Solid solid = TopoDS::Solid(sx.Current());
      const TopoDS_Shell outer = BRepClass3d::OuterShell(solid);
      for (TopoDS_Iterator it(solid); it.More(); it.Next()) {
        if (it.Value().ShapeType() != TopAbs_SHELL) {
          continue;
        }
        const bool cavity = !it.Value().IsSame(outer);
        for (TopExp_Explorer fx(it.Value(), TopAbs_FACE); fx.More(); fx.Next()) {
          const int fi = faces_.FindIndex(fx.Current());
          if (fi > 0) {
            face_solid_[fi - 1] = solid_index;
            face_cavity_[fi - 1] = cavity;
          }
        }
      }
    }
    for (int i = 1; i <= n; ++i) {
      const TopoDS_Face f = TopoDS::Face(faces_(i));
      BRepAdaptor_Surface sa(f, Standard_False);
      if (sa.GetType() != GeomAbs_Plane) {
        continue;
      }
      const gp_Pln pln = sa.Plane();
      const gp_Dir nrm = pln.Axis().Direction();
      for (int a = 0; a < 3; ++a) {
        if (std::fabs(nrm.Coord(a + 1)) < 1.0 - 1e-9) {
          continue;
        }
        gp_Pnt pmin(min_[0], min_[1], min_[2]);
        gp_Pnt pmax(max_[0], max_[1], max_[2]);
        if (pln.Distance(pmin) <= tol_) {
          cap_side_[i - 1] = a * 2;
        } else if (pln.Distance(pmax) <= tol_) {
          cap_side_[i - 1] = a * 2 + 1;
        }
      }
    }
  }

  // ---- instancing --------------------------------------------------------

  const TopoDS_Vertex &vertex_instance(int vi, const Cell &cell) {
    const Boundary &b = vertex_info_[vi];
    const Cell owner = shifted(cell, b.shift);
    const InstanceKey key{b.canon, pack_cell(owner.i, owner.j, owner.k)};
    auto it = vertex_instances_.find(key);
    if (it != vertex_instances_.end()) {
      return it->second;
    }
    const TopoDS_Vertex canon = TopoDS::Vertex(vertices_(b.canon + 1));
    gp_Pnt p = BRep_Tool::Pnt(canon).Transformed(trsf(owner));
    TopoDS_Vertex nv;
    builder_.MakeVertex(nv, p, BRep_Tool::Tolerance(canon));
    return vertex_instances_.emplace(key, nv).first->second;
  }

  // Non-degenerate edge instance keyed by its canonical edge and owning cell.
  const TopoDS_Edge &edge_instance(int ei, const Cell &cell) {
    const Boundary &b = edge_info_[ei];
    const Cell owner = shifted(cell, b.shift);
    const InstanceKey key{b.canon, pack_cell(owner.i, owner.j, owner.k)};
    auto it = edge_instances_.find(key);
    if (it != edge_instances_.end()) {
      return it->second;
    }
    const TopoDS_Edge canon = TopoDS::Edge(edges_(b.canon + 1));
    double fc, lc;
    Handle(Geom_Curve) c3 = BRep_Tool::Curve(canon, fc, lc);
    if (c3.IsNull()) {
      throw std::runtime_error("periodic cell: edge without 3D curve");
    }
    Handle(Geom_Curve) nc = Handle(Geom_Curve)::DownCast(c3->Transformed(trsf(owner)));
    TopoDS_Edge ne;
    builder_.MakeEdge(ne, nc, BRep_Tool::Tolerance(canon));
    builder_.Range(ne, fc, lc);

    TopoDS_Vertex v1, v2;
    TopExp::Vertices(canon, v1, v2);
    if (!v1.IsNull()) {
      const TopoDS_Vertex &nv1 = vertex_instance(vertices_.FindIndex(v1) - 1, owner);
      builder_.Add(ne, nv1.Oriented(TopAbs_FORWARD));
      builder_.UpdateVertex(nv1, fc, ne, BRep_Tool::Tolerance(v1));
    }
    if (!v2.IsNull()) {
      const TopoDS_Vertex &nv2 = vertex_instance(vertices_.FindIndex(v2) - 1, owner);
      builder_.Add(ne, nv2.Oriented(TopAbs_REVERSED));
      builder_.UpdateVertex(nv2, lc, ne, BRep_Tool::Tolerance(v2));
    }
    builder_.SameRange(ne, Standard_True);
    builder_.SameParameter(ne, Standard_True);
    return edge_instances_.emplace(key, ne).first->second;
  }

  // Re-express a pcurve given in the parameter of a non-canonical edge
  // ([f, l]) in the parameter of its canonical twin ([fc, lc], same length):
  // Q(s) = P(t(s)), t(s) = s - delta, or t(s) = l + fc - s when reversed.
  Handle(Geom2d_Curve) reparam_pcurve(const Handle(Geom2d_Curve) &pc, double f, double l,
                                      double delta, bool reversed) const {
    const double fc = f + delta;
    Handle(Geom2d_Curve) basis = pc;
    if (basis->IsKind(STANDARD_TYPE(Geom2d_TrimmedCurve))) {
      basis = Handle(Geom2d_TrimmedCurve)::DownCast(basis)->BasisCurve();
    }
    if (basis->IsKind(STANDARD_TYPE(Geom2d_Line))) {
      Handle(Geom2d_Line) ln = Handle(Geom2d_Line)::DownCast(basis);
      const gp_Pnt2d o = ln->Location();
      const gp_Dir2d d = ln->Direction();
      if (!reversed) {
        return new Geom2d_Line(gp_Pnt2d(o.X() - delta * d.X(), o.Y() - delta * d.Y()), d);
      }
      const double t0 = l + fc;
      return new Geom2d_Line(gp_Pnt2d(o.X() + t0 * d.X(), o.Y() + t0 * d.Y()), d.Reversed());
    }
    if (basis->IsKind(STANDARD_TYPE(Geom2d_Circle)) || basis->IsKind(STANDARD_TYPE(Geom2d_Ellipse))) {
      gp_Ax22d ax;
      double r1 = 0, r2 = 0;
      const bool circle = basis->IsKind(STANDARD_TYPE(Geom2d_Circle));
      if (circle) {
        Handle(Geom2d_Circle) c = Handle(Geom2d_Circle)::DownCast(basis);
        ax = c->Position();
        r1 = c->Radius();
      } else {
        Handle(Geom2d_Ellipse) e = Handle(Geom2d_Ellipse)::DownCast(basis);
        ax = e->Position();
        r1 = e->MajorRadius();
        r2 = e->MinorRadius();
      }
      const gp_Dir2d x = ax.XDirection();
      const gp_Dir2d y = ax.YDirection();
      double ca, sa;
      gp_Dir2d nx, ny;
      if (!reversed) {
        ca = std::cos(delta);
        sa = std::sin(delta);
        nx = gp_Dir2d(ca * x.X() + sa * y.X(), ca * x.Y() + sa * y.Y());
        ny = gp_Dir2d(-sa * x.X() + ca * y.X(), -sa * x.Y() + ca * y.Y());
      } else {
        const double t0 = l + fc;
        ca = std::cos(t0);
        sa = std::sin(t0);
        nx = gp_Dir2d(ca * x.X() + sa * y.X(), ca * x.Y() + sa * y.Y());
        ny = gp_Dir2d(sa * x.X() - ca * y.X(), sa * x.Y() - ca * y.Y());
      }
      const gp_Ax22d nax(ax.Location(), nx, ny);
      if (circle) {
        return new Geom2d_Circle(nax, r1);
      }
      return new Geom2d_Ellipse(nax, r1, r2);
    }
    // Anything else: exact for B-splines / Beziers (knot shift), an
    // approximation for the remaining conics (never produced by planar cuts).
    Handle(Geom2d_TrimmedCurve) trimmed = new Geom2d_TrimmedCurve(basis, f, l);
    Handle(Geom2d_BSplineCurve) bs = Geom2dConvert::CurveToBSplineCurve(trimmed);
    if (bs.IsNull()) {
      throw std::runtime_error("periodic cell: cannot convert pcurve to B-spline");
    }
    if (reversed) {
      bs->Reverse();
    }
    TColStd_Array1OfReal knots(1, bs->NbKnots());
    bs->Knots(knots);
    const double shift = fc - knots(1);
    for (int i = 1; i <= knots.Length(); ++i) {
      knots(i) += shift;
    }
    bs->SetKnots(knots);
    return bs;
  }

  // Edge orientation relative to the face's own frame (wire ∘ edge).
  static TopAbs_Orientation relative(const TopoDS_Wire &w, const TopoDS_Edge &e) {
    return TopAbs::Compose(w.Orientation(), e.Orientation());
  }

  TopoDS_Face instantiate_face(int fi, const Cell &cell) {
    const TopoDS_Face face = TopoDS::Face(faces_(fi));
    const gp_Trsf t = trsf(cell);

    Handle(Geom_Surface) s = BRep_Tool::Surface(face);
    Handle(Geom_Surface) ns = Handle(Geom_Surface)::DownCast(s->Transformed(t));
    TopoDS_Face nf;
    builder_.MakeFace(nf, ns, BRep_Tool::Tolerance(face));
    builder_.NaturalRestriction(nf, BRep_Tool::NaturalRestriction(face));

    std::set<const TopoDS_TShape *> pcurve_done;
    // An edge with two pcurves that the face uses only once (a seam line
    // bounding a partial cylinder) is attached with the one pcurve its
    // orientation selects; keeping both makes booleans treat it as a seam.
    std::unordered_map<const TopoDS_TShape *, int> uses;
    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
      ++uses[ex.Current().TShape().get()];
    }

    for (TopoDS_Iterator wit(face, Standard_False, Standard_True); wit.More(); wit.Next()) {
      if (wit.Value().ShapeType() != TopAbs_WIRE) {
        continue;
      }
      const TopoDS_Wire wire = TopoDS::Wire(wit.Value());
      TopoDS_Wire nw;
      builder_.MakeWire(nw);

      for (TopoDS_Iterator eit(wire, Standard_False, Standard_True); eit.More(); eit.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(eit.Value());
        const int ei = edges_.FindIndex(edge) - 1;
        if (ei < 0) {
          throw std::runtime_error("periodic cell: edge not found in template map");
        }
        const Boundary &b = edge_info_[ei];
        const TopAbs_Orientation rel = relative(wire, edge);

        if (b.degenerated) {
          add_degenerated(face, nf, nw, edge, rel, cell);
          continue;
        }

        const TopoDS_Edge &ne = edge_instance(ei, cell);
        const bool rev = b.reversed;
        const TopAbs_Orientation o = rev ? TopAbs::Reverse(edge.Orientation()) : edge.Orientation();
        builder_.Add(nw, ne.Oriented(o));

        if (pcurve_done.count(ne.TShape().get())) {
          continue;
        }
        pcurve_done.insert(ne.TShape().get());

        double f, l;
        BRep_Tool::Range(edge, f, l);
        const double tol_e = BRep_Tool::Tolerance(edge);
        const bool closed = BRep_Tool::IsClosed(edge, face) && uses[edge.TShape().get()] > 1;
        if (closed) {
          double pf, pl;
          Handle(Geom2d_Curve) p1 = BRep_Tool::CurveOnSurface(TopoDS::Edge(edge.Oriented(TopAbs_FORWARD)), face, pf, pl);
          Handle(Geom2d_Curve) p2 = BRep_Tool::CurveOnSurface(TopoDS::Edge(edge.Oriented(TopAbs_REVERSED)), face, pf, pl);
          if (p1.IsNull() || p2.IsNull()) {
            throw std::runtime_error("periodic cell: closed edge without both pcurves");
          }
          if (b.project && b.canon != ei) {
            throw std::runtime_error("periodic cell: closed twin edge parametrised differently");
          }
          if (b.canon != ei || rev) {
            p1 = reparam_pcurve(p1, f, l, b.delta, rev);
            p2 = reparam_pcurve(p2, f, l, b.delta, rev);
            if (rev) {
              std::swap(p1, p2);
            }
          }
          builder_.UpdateEdge(ne, p1, p2, nf, tol_e);
        } else {
          double pf, pl;
          Handle(Geom2d_Curve) pc = BRep_Tool::CurveOnSurface(TopoDS::Edge(edge.Oriented(rel)), face, pf, pl);
          if (pc.IsNull()) {
            throw std::runtime_error("periodic cell: edge without pcurve on its face");
          }
          if (b.project && b.canon != ei) {
            Handle(Geom2d_Curve) mapped = pcurve_through_twin(ne, edge, pc, f, l, cell);
            builder_.UpdateEdge(ne, mapped, nf, tol_e);
            builder_.UpdateEdge(ne, (std::max)(BRep_Tool::Tolerance(ne), pcurve_deviation(ne, nf, ns)));
          } else {
            if (b.canon != ei || rev) {
              pc = reparam_pcurve(pc, f, l, b.delta, rev);
            }
            builder_.UpdateEdge(ne, pc, nf, tol_e);
          }
        }
        verify_pcurve(ne, nf, ns);
      }
      builder_.Add(nf, nw.Oriented(wire.Orientation()));
    }

    nf.Orientation(face.Orientation());
    return nf;
  }

  // Degenerated edges belong to a single face: fresh edge per instance, with
  // the pole vertex shared through the vertex map.
  void add_degenerated(const TopoDS_Face &face, TopoDS_Face &nf, TopoDS_Wire &nw,
                       const TopoDS_Edge &edge, TopAbs_Orientation rel, const Cell &cell) {
    double pf, pl;
    Handle(Geom2d_Curve) pc = BRep_Tool::CurveOnSurface(TopoDS::Edge(edge.Oriented(rel)), face, pf, pl);
    if (pc.IsNull()) {
      throw std::runtime_error("periodic cell: degenerated edge without pcurve");
    }
    TopoDS_Edge ne;
    builder_.MakeEdge(ne);
    builder_.Degenerated(ne, Standard_True);
    builder_.UpdateEdge(ne, Handle(Geom2d_Curve)::DownCast(pc->Copy()), nf, BRep_Tool::Tolerance(edge));
    builder_.Range(ne, nf, pf, pl);

    for (TopoDS_Iterator vit(edge, Standard_False, Standard_True); vit.More(); vit.Next()) {
      const TopoDS_Vertex v = TopoDS::Vertex(vit.Value());
      const int vi = vertices_.FindIndex(v) - 1;
      const TopoDS_Vertex &nv = vertex_instance(vi, cell);
      builder_.Add(ne, nv.Oriented(v.Orientation()));
      const double param = BRep_Tool::Parameter(v, edge, face);
      builder_.UpdateVertex(nv, param, ne, nf, BRep_Tool::Tolerance(v));
    }
    builder_.Add(nw, ne.Oriented(edge.Orientation()));
  }

  // Check that the pcurve just attached matches the 3D curve at three
  // parameters; a mismatch means the periodic reparametrisation is wrong.
  // The face's own pcurve `pc` (over the parameter of `edge`, range [f, l])
  // re-expressed in the parameter of the twin instance `ne`, whose curve
  // runs along the same points with another parametrisation: each sample
  // of `ne` is projected onto the instance of `edge` to find its parameter
  // on `pc`, and the ends are pinned so the wire stays closed.
  Handle(Geom2d_Curve) pcurve_through_twin(const TopoDS_Edge &ne, const TopoDS_Edge &edge,
                                           const Handle(Geom2d_Curve) &pc, double f, double l,
                                           const Cell &cell) {
    double fc, lc;
    Handle(Geom_Curve) cn = BRep_Tool::Curve(ne, fc, lc);
    double ef, el;
    Handle(Geom_Curve) ce = BRep_Tool::Curve(edge, ef, el);
    if (cn.IsNull() || ce.IsNull()) {
      throw std::runtime_error("periodic cell: twin edge without 3D curve");
    }
    Handle(Geom_Curve) here = Handle(Geom_Curve)::DownCast(ce->Transformed(trsf(cell)));
    const int n = 33;
    Handle(TColgp_HArray1OfPnt2d) points = new TColgp_HArray1OfPnt2d(1, n);
    Handle(TColStd_HArray1OfReal) params = new TColStd_HArray1OfReal(1, n);
    const bool rev = here->Value(f).Distance(cn->Value(fc)) > here->Value(l).Distance(cn->Value(fc));
    for (int k = 0; k < n; ++k) {
      const double t = fc + (lc - fc) * k / (n - 1);
      double u;
      if (k == 0) {
        u = rev ? l : f;
      } else if (k == n - 1) {
        u = rev ? f : l;
      } else {
        GeomAPI_ProjectPointOnCurve proj(cn->Value(t), here, f, l);
        if (proj.NbPoints() == 0) {
          throw std::runtime_error("periodic cell: twin edge sample off its partner");
        }
        u = proj.LowerDistanceParameter();
      }
      points->SetValue(k + 1, pc->Value(u));
      params->SetValue(k + 1, t);
    }
    Geom2dAPI_Interpolate interpolate(points, params, Standard_False, Precision::PConfusion());
    interpolate.Perform();
    if (!interpolate.IsDone()) {
      throw std::runtime_error("periodic cell: twin pcurve interpolation failed");
    }
    return interpolate.Curve();
  }

  // Largest distance between the edge's curve and its pcurve on `nf`.
  static double pcurve_deviation(const TopoDS_Edge &ne, const TopoDS_Face &nf, const Handle(Geom_Surface) &ns) {
    double f, l;
    Handle(Geom_Curve) c3 = BRep_Tool::Curve(ne, f, l);
    double pf, pl;
    Handle(Geom2d_Curve) pc = BRep_Tool::CurveOnSurface(ne, nf, pf, pl);
    if (c3.IsNull() || pc.IsNull()) {
      return 0.0;
    }
    double worst = 0.0;
    for (int s = 0; s <= 16; ++s) {
      const double u = f + (l - f) * s / 16.0;
      const gp_Pnt2d uv = pc->Value(u);
      worst = (std::max)(worst, ns->Value(uv.X(), uv.Y()).Distance(c3->Value(u)));
    }
    return 1.01 * worst;
  }

  void verify_pcurve(const TopoDS_Edge &ne, const TopoDS_Face &nf, const Handle(Geom_Surface) &ns) const {
    double f, l;
    Handle(Geom_Curve) c3 = BRep_Tool::Curve(ne, f, l);
    double pf, pl;
    Handle(Geom2d_Curve) pc = BRep_Tool::CurveOnSurface(ne, nf, pf, pl);
    if (c3.IsNull() || pc.IsNull()) {
      return;
    }
    if (std::getenv("NODI_TPMS_DEBUG") != nullptr &&
        (pc->FirstParameter() > f + 1e-9 || pc->LastParameter() < l - 1e-9 || pf > f + 1e-9 || pl < l - 1e-9)) {
      const gp_Pnt mid = c3->Value(0.5 * (f + l));
      std::cerr << "periodic cell: pcurve range [" << pc->FirstParameter() << ", " << pc->LastParameter()
                << "] stored [" << pf << ", " << pl << "] vs edge [" << f << ", " << l << "] at (" << mid.X()
                << ", " << mid.Y() << ", " << mid.Z() << ")" << std::endl;
    }
    const double limit = 10.0 * (std::max)(tol_, BRep_Tool::Tolerance(ne));
    const double params[3] = {f, 0.5 * (f + l), l};
    for (double u : params) {
      const gp_Pnt2d uv = pc->Value(u);
      const gp_Pnt on_surface = ns->Value(uv.X(), uv.Y());
      const double d = on_surface.Distance(c3->Value(u));
      if (d > limit) {
        std::ostringstream msg;
        msg << "periodic cell: pcurve deviates from edge by " << d << " (limit " << limit << ")";
        throw std::runtime_error(msg.str());
      }
    }
  }

  // ---- result ------------------------------------------------------------

  std::unique_ptr<TopoDS_Shape> build_result(std::string &report) {
    const size_t n = out_faces_.size();
    if (n == 0) {
      throw std::runtime_error("periodic cell: no faces to assemble");
    }
    // Union-find over faces sharing an edge.
    std::vector<size_t> parent(n);
    for (size_t i = 0; i < n; ++i) {
      parent[i] = i;
    }
    auto find = [&](size_t x) {
      while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
      }
      return x;
    };
    std::vector<std::vector<const TopoDS_TShape *>> face_edges(n);
    for (size_t i = 0; i < n; ++i) {
      for (TopExp_Explorer ex(out_faces_[i], TopAbs_EDGE); ex.More(); ex.Next()) {
        face_edges[i].push_back(ex.Current().TShape().get());
      }
    }
    std::unordered_map<const TopoDS_TShape *, std::vector<size_t>> edge_faces;
    for (size_t i = 0; i < n; ++i) {
      std::set<const TopoDS_TShape *> uniq(face_edges[i].begin(), face_edges[i].end());
      for (const TopoDS_TShape *e : uniq) {
        auto &lst = edge_faces[e];
        if (!lst.empty()) {
          parent[find(lst.front())] = find(i);
        }
        lst.push_back(i);
      }
    }
    // Free edges: referenced by exactly one face and not degenerated / seam.
    std::unordered_map<size_t, int> free_per_component;
    for (const auto &kv : edge_faces) {
      if (kv.second.size() == 1) {
        const size_t fi = kv.second.front();
        int count = 0;
        for (const TopoDS_TShape *e : face_edges[fi]) {
          count += e == kv.first;
        }
        if (count == 1) {
          const TopoDS_Edge probe = TopoDS::Edge(find_edge(fi, kv.first));
          if (!BRep_Tool::Degenerated(probe)) {
            free_per_component[find(fi)] += 1;
          }
        }
      }
    }

    std::unordered_map<size_t, TopoDS_Shell> shells;
    std::vector<size_t> order;
    for (size_t i = 0; i < n; ++i) {
      const size_t root = find(i);
      auto it = shells.find(root);
      if (it == shells.end()) {
        TopoDS_Shell sh;
        builder_.MakeShell(sh);
        it = shells.emplace(root, sh).first;
        order.push_back(root);
      }
      builder_.Add(it->second, out_faces_[i]);
    }

    // Provenance: an instanced cavity shell stays a cavity of the component
    // holding its solid's outer faces in that cell.
    std::vector<size_t> members_root(n);
    std::unordered_map<size_t, bool> all_cavity;
    std::unordered_set<size_t> has_cap;
    std::unordered_map<size_t, std::pair<int64_t, int>> cavity_origin;
    std::map<std::pair<int64_t, int>, size_t> outer_root;
    for (size_t i = 0; i < n; ++i) {
      const size_t root = find(i);
      members_root[i] = root;
      auto it = all_cavity.find(root);
      if (it == all_cavity.end()) {
        all_cavity.emplace(root, out_cavity_[i]);
        cavity_origin.emplace(root, out_origin_[i]);
      } else if (!out_cavity_[i]) {
        it->second = false;
      }
      if (out_cap_[i]) {
        has_cap.insert(root);
      }
      if (!out_cavity_[i] && out_origin_[i].second >= 0) {
        outer_root.emplace(out_origin_[i], root);
      }
    }

    int solids = 0, open = 0, free_edges = 0;
    std::vector<TopoDS_Shape> parts;
    std::vector<TopoDS_Shell> closed;
    std::vector<size_t> closed_roots;
    for (size_t root : order) {
      TopoDS_Shell &sh = shells[root];
      const int free_count = free_per_component[root];
      if (free_count == 0) {
        sh.Closed(Standard_True);
        closed.push_back(sh);
        closed_roots.push_back(root);
      } else {
        parts.push_back(sh);
        ++open;
        free_edges += free_count;
      }
    }
    std::vector<int> known(closed.size(), -2);
    std::unordered_map<size_t, int> closed_index;
    for (size_t c = 0; c < closed_roots.size(); ++c) {
      closed_index.emplace(closed_roots[c], int(c));
    }
    for (size_t c = 0; c < closed_roots.size(); ++c) {
      const size_t root = closed_roots[c];
      if (!all_cavity[root]) {
        continue;
      }
      auto host = outer_root.find(cavity_origin[root]);
      if (host == outer_root.end()) {
        continue;
      }
      auto hc = closed_index.find(host->second);
      if (hc != closed_index.end()) {
        known[c] = hc->second;
      }
    }
    // A shell with a face on the block's boundary bounds a solid; a shell of
    // template faces only may be a void pocket spanning cells and is tested.
    for (size_t c = 0; c < known.size(); ++c) {
      if (known[c] == -2 && has_cap.count(closed_roots[c]) > 0) {
        known[c] = -1;
      }
    }
    if (std::getenv("NODI_TPMS_DEBUG") != nullptr) {
      int cavity_faces = 0, cavity_shells = 0, hosted = 0, template_cavity = 0;
      for (bool c : out_cavity_) {
        cavity_faces += c;
      }
      for (bool c : face_cavity_) {
        template_cavity += c;
      }
      for (size_t c = 0; c < closed_roots.size(); ++c) {
        cavity_shells += all_cavity[closed_roots[c]];
        hosted += known[c] >= 0;
      }
      std::cerr << "periodic cell: template cavity faces " << template_cavity << ", output cavity faces "
                << cavity_faces << ", closed shells " << closed.size() << ", cavity shells " << cavity_shells
                << ", hosted " << hosted << std::endl;
    }
    for (const TopoDS_Shape &so : nest_shells(closed, tol_, known)) {
      parts.push_back(so);
      ++solids;
    }

    std::ostringstream rep;
    rep << "faces=" << n << " solids=" << solids << " open_shells=" << open << " free_edges=" << free_edges;
    report += rep.str();

    if (parts.size() == 1) {
      return std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape(parts.front()));
    }
    TopoDS_Compound comp;
    builder_.MakeCompound(comp);
    for (const TopoDS_Shape &p : parts) {
      builder_.Add(comp, p);
    }
    return std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape(comp));
  }

public:
  // For every face of `result` (explorer order): the index of the earlier
  // face it is a translated copy of, or -1, and that translation. Only the
  // faces of kept cells whose 26 neighbours are kept are copies: everything
  // they share (down to the vertices on the cell corners) is template
  // topology, so a mesh of the copy stitches to its neighbours' like the
  // representative's does.
  void instancing(const TopoDS_Shape &result, std::vector<int32_t> &reps, std::vector<double> &offsets) const {
    std::unordered_map<const TopoDS_TShape *, size_t> index;
    for (size_t i = 0; i < out_faces_.size(); ++i) {
      index.emplace(out_faces_[i].TShape().get(), i);
    }
    auto interior = [&](const Cell &c) {
      for (int di = -1; di <= 1; ++di) {
        for (int dj = -1; dj <= 1; ++dj) {
          for (int dk = -1; dk <= 1; ++dk) {
            if (class_of(Cell{c.i + di, c.j + dj, c.k + dk}) != Class::Keep) {
              return false;
            }
          }
        }
      }
      return true;
    };
    std::unordered_map<int, std::pair<int32_t, Cell>> first; // template face -> (result index, cell)
    int32_t at = 0;
    for (TopExp_Explorer ex(result, TopAbs_FACE); ex.More(); ex.Next(), ++at) {
      int32_t rep = -1;
      double d[3] = {0.0, 0.0, 0.0};
      auto it = index.find(ex.Current().TShape().get());
      if (it != index.end() && out_template_[it->second].first >= 0 && interior(out_template_[it->second].second)) {
        const int fi = out_template_[it->second].first;
        const Cell &c = out_template_[it->second].second;
        auto seen = first.find(fi);
        if (seen == first.end()) {
          first.emplace(fi, std::make_pair(at, c));
        } else {
          rep = seen->second.first;
          const Cell &r = seen->second.second;
          d[0] = (c.i - r.i) * period_[0];
          d[1] = (c.j - r.j) * period_[1];
          d[2] = (c.k - r.k) * period_[2];
        }
      }
      reps.push_back(rep);
      offsets.insert(offsets.end(), d, d + 3);
    }
  }

private:
  TopoDS_Shape find_edge(size_t face_index, const TopoDS_TShape *tshape) const {
    for (TopExp_Explorer ex(out_faces_[face_index], TopAbs_EDGE); ex.More(); ex.Next()) {
      if (ex.Current().TShape().get() == tshape) {
        return ex.Current();
      }
    }
    return TopoDS_Shape();
  }
};

} // namespace periodic_lattice

inline std::vector<periodic_lattice::Cell> parse_cells(rust::Slice<const double> bounds,
                                                       rust::Slice<const int32_t> cells) {
  if (bounds.size() != 6) {
    throw std::runtime_error("bounds must hold 6 values");
  }
  if (cells.size() % 3 != 0 || cells.empty()) {
    throw std::runtime_error("cells must hold (i, j, k) triples");
  }
  std::vector<periodic_lattice::Cell> list;
  list.reserve(cells.size() / 3);
  for (size_t i = 0; i + 2 < cells.size(); i += 3) {
    list.push_back(periodic_lattice::Cell{cells[i], cells[i + 1], cells[i + 2]});
  }
  return list;
}

// Widen vertex tolerances to reach the ends of their edges' curves, so that
// edges substituted from a neighbouring cell (equal within the stitching
// tolerance, not exactly) leave the shape valid.
inline void heal_vertex_tolerances(const TopoDS_Shape &shape) {
  BRep_Builder builder;
  double worst_curve = 0.0, worst_pcurve = 0.0;
  double *worst = &worst_curve;
  auto widen = [&](const TopoDS_Vertex &vertex, double gap) {
    *worst = (std::max)(*worst, gap);
    if (gap > BRep_Tool::Tolerance(vertex)) {
      builder.UpdateVertex(vertex, 1.01 * gap);
    }
  };
  for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
    const TopoDS_Edge edge = TopoDS::Edge(ex.Current());
    double f, l;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, f, l);
    if (curve.IsNull()) {
      continue;
    }
    for (TopoDS_Iterator it(edge, Standard_False, Standard_True); it.More(); it.Next()) {
      if (it.Value().ShapeType() != TopAbs_VERTEX) {
        continue;
      }
      const TopoDS_Vertex vertex = TopoDS::Vertex(it.Value());
      const double param = BRep_Tool::Parameter(vertex, edge);
      widen(vertex, BRep_Tool::Pnt(vertex).Distance(curve->Value(param)));
    }
  }
  // The pcurve ends on each face as well.
  worst = &worst_pcurve;
  for (TopExp_Explorer fx(shape, TopAbs_FACE); fx.More(); fx.Next()) {
    const TopoDS_Face face = TopoDS::Face(fx.Current());
    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    if (surface.IsNull()) {
      continue;
    }
    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
      const TopoDS_Edge edge = TopoDS::Edge(ex.Current());
      double f, l;
      Handle(Geom2d_Curve) pc = BRep_Tool::CurveOnSurface(edge, face, f, l);
      if (pc.IsNull()) {
        continue;
      }
      for (TopoDS_Iterator it(edge, Standard_False, Standard_True); it.More(); it.Next()) {
        if (it.Value().ShapeType() != TopAbs_VERTEX) {
          continue;
        }
        const TopoDS_Vertex vertex = TopoDS::Vertex(it.Value());
        const gp_Pnt2d uv = pc->Value(BRep_Tool::Parameter(vertex, edge));
        widen(vertex, BRep_Tool::Pnt(vertex).Distance(surface->Value(uv.X(), uv.Y())));
      }
    }
  }
  if (std::getenv("NODI_TPMS_DEBUG") != nullptr) {
    double edge = 0.0, vertex = 0.0;
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
      edge = (std::max)(edge, BRep_Tool::Tolerance(TopoDS::Edge(ex.Current())));
    }
    for (TopExp_Explorer ex(shape, TopAbs_VERTEX); ex.More(); ex.Next()) {
      vertex = (std::max)(vertex, BRep_Tool::Tolerance(TopoDS::Vertex(ex.Current())));
    }
    std::cerr << "heal_vertex_tolerances: worst gap to curve ends " << worst_curve << ", to pcurve ends "
              << worst_pcurve << "; max edge tolerance " << edge << ", max vertex tolerance " << vertex
              << std::endl;
  }
}

// `bounds` = [min_x, min_y, min_z, max_x, max_y, max_z] of the template cell,
// `cells` = flat (i, j, k) triples relative to the template (cell (0,0,0) is
// the template itself). Throws on a non-periodic template; `report` receives
// face / solid / free-edge counts.
inline void export_instancing(const periodic_lattice::Assembler &assembler, const TopoDS_Shape &shape,
                              rust::Vec<int32_t> &reps, rust::Vec<double> &offsets) {
  std::vector<int32_t> r;
  std::vector<double> o;
  assembler.instancing(shape, r, o);
  for (int32_t v : r) {
    reps.push_back(v);
  }
  for (double v : o) {
    offsets.push_back(v);
  }
}

inline std::unique_ptr<TopoDS_Shape> assemble_periodic_cells(const TopoDS_Shape &cell,
                                                             rust::Slice<const double> bounds,
                                                             rust::Slice<const int32_t> cells,
                                                             double tolerance, rust::String &report,
                                                             rust::Vec<int32_t> &reps, rust::Vec<double> &offsets) {
  const auto list = parse_cells(bounds, cells);
  periodic_lattice::Assembler assembler(cell, bounds.data(), tolerance);
  std::string rep;
  auto shape = assembler.assemble(list, nullptr, false, rep);
  heal_vertex_tolerances(*shape);
  export_instancing(assembler, *shape, reps, offsets);
  report = rust::String(rep);
  return shape;
}

// As `assemble_periodic_cells`, keeping only the lattice material inside
// `body` (`keep_outside` = false) or outside it (true). `body` lives in the
// template's coordinates; its sub-shape tolerances may be raised.
inline std::unique_ptr<TopoDS_Shape> clip_periodic_cells(const TopoDS_Shape &cell,
                                                         rust::Slice<const double> bounds,
                                                         rust::Slice<const int32_t> cells,
                                                         const TopoDS_Shape &body, bool keep_outside,
                                                         double tolerance, rust::String &report,
                                                         rust::Vec<int32_t> &reps, rust::Vec<double> &offsets) {
  const auto list = parse_cells(bounds, cells);
  periodic_lattice::Assembler assembler(cell, bounds.data(), tolerance);
  std::string rep;
  auto shape = assembler.assemble(list, &body, keep_outside, rep);
  heal_vertex_tolerances(*shape);
  export_instancing(assembler, *shape, reps, offsets);
  report = rust::String(rep);
  return shape;
}

// A point strictly inside `solid`: the first point of a grid over its
// bounding box (refined up to 32 per axis) that BRepClass3d classifies IN.
// Throws when none is found.
inline std::unique_ptr<gp_Pnt> point_inside_solid(const TopoDS_Shape &solid, double tolerance) {
  Bnd_Box box;
  BRepBndLib::Add(solid, box, Standard_False);
  if (box.IsVoid()) {
    throw std::runtime_error("point_inside_solid: empty shape");
  }
  double lo[3], hi[3];
  box.Get(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
  const double diagonal = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) +
                                    (hi[2] - lo[2]) * (hi[2] - lo[2]));
  BRepClass3d_SolidClassifier classifier(solid);
  // A point of a face moved against its outward normal.
  for (TopExp_Explorer ex(solid, TopAbs_FACE); ex.More(); ex.Next()) {
    const TopoDS_Face &face = TopoDS::Face(ex.Current());
    double u0, u1, v0, v1;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    BRepAdaptor_Surface surface(face, Standard_False);
    BRepClass_FaceClassifier on_face;
    for (int n = 2; n <= 8; n *= 2) {
      for (int i = 1; i < n; ++i) {
        for (int j = 1; j < n; ++j) {
          const gp_Pnt2d uv(u0 + (u1 - u0) * i / n, v0 + (v1 - v0) * j / n);
          on_face.Perform(face, uv, tolerance);
          if (on_face.State() != TopAbs_IN) {
            continue;
          }
          gp_Pnt p;
          gp_Vec du, dv;
          surface.D1(uv.X(), uv.Y(), p, du, dv);
          gp_Vec normal = du.Crossed(dv);
          if (normal.Magnitude() < 1e-300) {
            continue;
          }
          normal.Normalize();
          if (face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
          }
          for (double step = 1e-3 * diagonal; step > 1e-8 * diagonal; step *= 0.1) {
            const gp_Pnt q = p.Translated(-step * normal);
            classifier.Perform(q, tolerance);
            if (classifier.State() == TopAbs_IN) {
              return std::unique_ptr<gp_Pnt>(new gp_Pnt(q));
            }
          }
        }
      }
    }
  }
  for (int n = 3; n <= 33; n = n * 2 - 1) {
    for (int i = 1; i < n; ++i) {
      for (int j = 1; j < n; ++j) {
        for (int k = 1; k < n; ++k) {
          const gp_Pnt p(lo[0] + (hi[0] - lo[0]) * i / n, lo[1] + (hi[1] - lo[1]) * j / n,
                         lo[2] + (hi[2] - lo[2]) * k / n);
          classifier.Perform(p, tolerance);
          if (classifier.State() == TopAbs_IN) {
            return std::unique_ptr<gp_Pnt>(new gp_Pnt(p));
          }
        }
      }
    }
  }
  throw std::runtime_error("point_inside_solid: no interior point found");
}

inline bool brep_check_valid(const TopoDS_Shape &shape) {
  BRepCheck_Analyzer analyzer(shape);
  return analyzer.IsValid() == Standard_True;
}

// Every sub-shape BRepCheck_Analyzer flags, with its statuses and a point on
// it (at most 40 entries).
inline rust::String brep_check_report(const TopoDS_Shape &shape) {
  BRepCheck_Analyzer analyzer(shape);
  std::ostringstream out;
  if (analyzer.IsValid()) {
    return rust::String("valid");
  }
  int listed = 0;
  TopTools_IndexedMapOfShape all;
  TopExp::MapShapes(shape, all);
  for (int i = 1; i <= all.Extent() && listed < 40; ++i) {
    const TopoDS_Shape &sub = all(i);
    const Handle(BRepCheck_Result) &res = analyzer.Result(sub);
    if (res.IsNull()) {
      continue;
    }
    bool bad = false;
    std::ostringstream statuses;
    for (BRepCheck_ListIteratorOfListOfStatus it(res->Status()); it.More(); it.Next()) {
      if (it.Value() != BRepCheck_NoError) {
        bad = true;
        BRepCheck::Print(it.Value(), statuses);
      }
    }
    // Statuses in the context of ancestors (edge in face, wire in face...).
    for (res->InitContextIterator(); res->MoreShapeInContext(); res->NextShapeInContext()) {
      for (BRepCheck_ListIteratorOfListOfStatus it(res->StatusOnShape()); it.More(); it.Next()) {
        if (it.Value() != BRepCheck_NoError) {
          bad = true;
          statuses << "[in " << TopAbs::ShapeTypeToString(res->ContextualShape().ShapeType()) << "] ";
          BRepCheck::Print(it.Value(), statuses);
        }
      }
    }
    if (!bad) {
      continue;
    }
    ++listed;
    Bnd_Box box;
    BRepBndLib::Add(sub, box, Standard_False);
    double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
    if (!box.IsVoid()) {
      box.Get(x0, y0, z0, x1, y1, z1);
    }
    out << TopAbs::ShapeTypeToString(sub.ShapeType()) << " (" << x0 << "," << y0 << "," << z0 << ")-(" << x1 << ","
        << y1 << "," << z1 << "): " << statuses.str() << '\n';
  }
  return rust::String(out.str());
}

// Edges referenced by exactly one face (degenerated edges excluded).
inline int32_t count_free_edges(const TopoDS_Shape &shape) {
  return periodic_lattice::count_free_edges_of(shape);
}

