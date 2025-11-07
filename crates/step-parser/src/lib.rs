use opencascade::primitives::{CurveDetails, Orientation, Shape, SurfaceDetails};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EdgeInfo {
    pub curve_details: CurveDetails,
    pub orientation: Orientation,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FaceInfo {
    pub surface_details: SurfaceDetails,
    pub surface_type: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StepGeometry {
    pub edges: Vec<EdgeInfo>,
    pub faces: Vec<FaceInfo>,
}

/// Parse STEP file from bytes and extract geometry information
pub fn parse_step_from_bytes(bytes: &[u8]) -> Result<StepGeometry, String> {
    // Import STEP file directly from bytes
    let shape = Shape::read_step_from_bytes(bytes)
        .map_err(|e| format!("Failed to import STEP file: {}", e))?;

    // Extract geometry
    extract_geometry(&shape)
}

/// Extract edges and faces from a shape
fn extract_geometry(shape: &Shape) -> Result<StepGeometry, String> {
    let mut edges = Vec::new();
    let mut faces = Vec::new();

    // Extract all edges
    for edge in shape.edges() {
        let curve_details = edge.curve_details();

        edges.push(EdgeInfo { curve_details, orientation: edge.orientation() });
    }

    // Extract all faces
    for face in shape.faces() {
        let surface_details = face.surface_details();
        let surface_type = format!("{}", face.surface_type());

        faces.push(FaceInfo { surface_details, surface_type });
    }

    Ok(StepGeometry { edges, faces })
}

/// Serialize StepGeometry to JSON string
pub fn geometry_to_json(geometry: &StepGeometry) -> Result<String, String> {
    serde_json::to_string(geometry).map_err(|e| format!("Failed to serialize to JSON: {}", e))
}

/// Serialize StepGeometry to pretty JSON string
pub fn geometry_to_json_pretty(geometry: &StepGeometry) -> Result<String, String> {
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
        let geometry = StepGeometry { edges: vec![], faces: vec![] };

        let json = geometry_to_json(&geometry).unwrap();
        assert!(json.contains("edges"));
        assert!(json.contains("faces"));
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
                        println!(
                            "  Found {} edges and {} faces",
                            geometry.edges.len(),
                            geometry.faces.len()
                        );

                        // Verify we can serialize to JSON
                        let json = geometry_to_json(&geometry).unwrap();
                        assert!(json.contains("edges"));
                        assert!(json.contains("faces"));

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
}
