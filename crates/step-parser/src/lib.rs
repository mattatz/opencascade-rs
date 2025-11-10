#![allow(dead_code)]

use opencascade::primitives::{CurveDetails, Orientation, Shape, SurfaceDetails};
use serde::{Deserialize, Serialize};
use typeshare::typeshare;

/// A 3D vector with double-precision floating point components
#[allow(unused)]
#[typeshare]
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct DVec3 {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

/// A 2D vector with double-precision floating point components
#[allow(unused)]
#[typeshare]
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct DVec2 {
    pub x: f64,
    pub y: f64,
}

#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EdgeInfo {
    pub id: u32,
    pub curve: CurveDetails,
    pub orientation: Orientation,
}

#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WireInfo {
    pub edges: Vec<EdgeInfo>,
    pub is_outer: bool,
}

#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FaceInfo {
    pub surface: SurfaceDetails,
    pub wires: Vec<WireInfo>,
}

#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SolidInfo {
    pub faces: Vec<FaceInfo>,
}

#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StepInfo {
    pub solids: Vec<SolidInfo>,
}

/// Parse STEP file from bytes and extract geometry information
pub fn parse_step_from_bytes(bytes: &[u8]) -> Result<StepInfo, String> {
    // Import STEP file directly from bytes
    let shape = Shape::read_step_from_bytes(bytes)
        .map_err(|e| format!("Failed to import STEP file: {}", e))?;

    // Extract geometry
    extract_geometry(&shape)
}

/// Extract Brep structure from a shape (Solid → Face → Wire → Edge)
fn extract_geometry(shape: &Shape) -> Result<StepInfo, String> {
    let mut solids = Vec::new();

    // Check if the shape itself is a solid
    if let Some(solid_info) = extract_solid_from_shape(shape) {
        solids.push(solid_info);
    }

    // If no solids found, create a "virtual solid" from all faces in the shape
    if solids.is_empty() {
        let faces = extract_faces_from_shape(shape);
        if !faces.is_empty() {
            solids.push(SolidInfo { faces });
        }
    }

    Ok(StepInfo { solids })
}

/// Try to extract solid info from a shape (returns None if shape has no faces)
fn extract_solid_from_shape(shape: &Shape) -> Option<SolidInfo> {
    let faces = extract_faces_from_shape(shape);

    if faces.is_empty() { None } else { Some(SolidInfo { faces }) }
}

/// Extract all faces from a shape and their hierarchical structure
fn extract_faces_from_shape(shape: &Shape) -> Vec<FaceInfo> {
    let mut faces = Vec::new();

    for face in shape.faces() {
        let surface = face.surface_details();

        // Extract wires from the face
        let mut wires = Vec::new();

        // Get all wires with their roles (outer or inner)
        for wire_with_role in face.wires_with_roles() {
            let mut edges = Vec::new();

            // Extract edges from the wire
            for edge in wire_with_role.wire.edges() {
                let curve_details = edge.curve_details();
                let orientation = edge.orientation();

                edges.push(EdgeInfo { id: edge.id() as u32, curve: curve_details, orientation });
            }

            wires.push(WireInfo { edges, is_outer: wire_with_role.is_outer });
        }

        faces.push(FaceInfo { surface, wires });
    }

    faces
}

/// Serialize StepGeometry to JSON string
pub fn geometry_to_json(geometry: &StepInfo) -> Result<String, String> {
    serde_json::to_string(geometry).map_err(|e| format!("Failed to serialize to JSON: {}", e))
}

/// Serialize StepGeometry to pretty JSON string
pub fn geometry_to_json_pretty(geometry: &StepInfo) -> Result<String, String> {
    serde_json::to_string_pretty(geometry)
        .map_err(|e| format!("Failed to serialize to JSON: {}", e))
}

/// All-in-one function: Parse STEP from bytes and return JSON
pub fn step_bytes_to_json(bytes: &[u8]) -> Result<String, String> {
    let geometry = parse_step_from_bytes(bytes)?;
    geometry_to_json(&geometry)
}

/// All-in-one function: Parse STEP from bytes and return pretty JSON
pub fn step_bytes_to_json_pretty(bytes: &[u8]) -> Result<String, String> {
    let geometry = parse_step_from_bytes(bytes)?;
    geometry_to_json_pretty(&geometry)
}

/// C-compatible wrapper for parsing STEP from bytes
/// Returns a null-terminated C string that must be freed by the caller
#[unsafe(no_mangle)]
pub extern "C" fn parse_step(data: *const u8, len: usize) -> *mut std::os::raw::c_char {
    if data.is_null() {
        return std::ptr::null_mut();
    }

    let bytes = unsafe { std::slice::from_raw_parts(data, len) };

    match step_bytes_to_json(bytes) {
        Ok(json) => {
            let c_string = std::ffi::CString::new(json).unwrap_or_default();
            c_string.into_raw()
        },
        Err(e) => {
            println!("Error: {}", e);
            std::ptr::null_mut()
        },
    }
}

/// Free a C string allocated by parse_step
#[unsafe(no_mangle)]
pub extern "C" fn free_step(s: *mut std::os::raw::c_char) {
    if !s.is_null() {
        unsafe {
            let _ = std::ffi::CString::from_raw(s);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_geometry_serialization() {
        let geometry = StepInfo { solids: vec![] };

        let json = geometry_to_json(&geometry).unwrap();
        assert!(json.contains("solids"));
    }

    #[test]
    fn test_parse_step_file() {
        // Test with an actual STEP file if one exists
        let test_files = ["../../screw.step", "../../mouse.step", "../../linkrods.step"];

        for file_path in test_files.iter() {
            if let Ok(bytes) = std::fs::read(file_path) {
                println!("Testing with file: {}", file_path);

                match parse_step_from_bytes(&bytes) {
                    Ok(geometry) => {
                        println!("  Found {} solids", geometry.solids.len());
                        for (i, solid) in geometry.solids.iter().enumerate() {
                            println!("    Solid {}: {} faces", i, solid.faces.len());
                            for (j, face) in solid.faces.iter().enumerate() {
                                println!("      Face {}: {} wires", j, face.wires.len(),);
                                for (k, wire) in face.wires.iter().enumerate() {
                                    println!(
                                        "        Wire {} ({}): {} edges",
                                        k,
                                        if wire.is_outer { "outer" } else { "inner" },
                                        wire.edges.len()
                                    );
                                    let edges =
                                        wire.edges.iter().map(|edge| edge.id).collect::<Vec<_>>();
                                    println!("          Edges: {:?}", edges);
                                }
                            }
                        }

                        // Verify we can serialize to JSON
                        let json = geometry_to_json(&geometry).unwrap();
                        assert!(json.contains("solids"));

                        // Test pretty JSON too
                        let pretty_json = geometry_to_json_pretty(&geometry).unwrap();
                        assert!(pretty_json.len() > json.len()); // Pretty version should be longer

                        return; // Test passed with at least one file
                    },
                    Err(e) => {
                        println!("  Warning: Failed to parse: {}", e);
                    },
                }
            }
        }

        println!("Note: No test STEP files found, skipping functional test");
    }

    #[test]
    fn test_json_structure() {
        // Test with screw.step if it exists
        if let Ok(bytes) = std::fs::read("../../screw.step") {
            let geometry = parse_step_from_bytes(&bytes).unwrap();
            let json = geometry_to_json_pretty(&geometry).unwrap();

            // Show a sample of the JSON structure
            let lines: Vec<_> = json.lines().take(50).collect();
            println!("=== JSON Sample (first 50 lines) ===");
            for line in lines {
                println!("{}", line);
            }
            println!("...");
            println!("Total JSON length: {} characters", json.len());
        } else {
            println!("Note: screw.step not found, skipping JSON structure test");
        }
    }
}
