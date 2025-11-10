use serde::{Deserialize, Serialize};
use typeshare::typeshare;

use crate::{CurveDetails, Orientation, SurfaceDetails};

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
