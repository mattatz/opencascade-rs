use crate::{
    edge_drawer::{EdgeDrawer, LineBuilder, RenderedLine, SegmentInstance},
    surface_drawer::{CadMesh, SurfaceDrawer},
    wasm_engine::WasmEngine,
};
use anyhow::Error;
use camera::OrbitCamera;
use clap::Parser;
use glam::{vec2, vec4, DVec3, Mat4, Quat, Vec2, Vec3};
use kicad_parser::board::BoardLayer;
use opencascade::{
    kicad::KicadPcb,
    primitives::{IntoShape, Shape},
};
use simple_game::{
    graphics::{
        text::{AxisAlign, StyledText, TextAlignment, TextSystem},
        DepthTexture, GraphicsDevice,
    },
    util::FPSCounter,
    GameApp,
};
use smaa::{SmaaMode, SmaaTarget};
use std::path::PathBuf;
use winit::{
    dpi::PhysicalPosition,
    event::{ElementState, KeyEvent, MouseButton, MouseScrollDelta::PixelDelta, WindowEvent},
    event_loop::ActiveEventLoop,
    keyboard::{KeyCode, PhysicalKey},
    window::Window,
};

mod camera;
mod edge_drawer;
mod surface_drawer;
mod wasm_engine;

// Multipliers to convert mouse position deltas to a more intuitve camera perspective change.
const ZOOM_MULTIPLIER: f32 = 5.0;
const TOUCHPAD_ZOOM_MULTIPLIER: f32 = 0.5;
const ROTATE_MULTIPLIER: f32 = 8.0;
const TOUCHPAD_ROTATE_MULTIPLIER: f32 = -0.05;
const PAN_MULTIPLIER: f32 = 150.0;
const TOUCHPAD_PAN_MULTIPLIER: f32 = 100.0;

#[derive(Default)]
struct MouseState {
    left_button_down: bool,
    middle_button_down: bool,
    right_button_down: bool,
    last_position: PhysicalPosition<f64>,
}

impl MouseState {
    fn delta(&mut self, position: PhysicalPosition<f64>) -> (f64, f64) {
        let delta = (position.x - self.last_position.x, position.y - self.last_position.y);
        self.last_position = position;
        delta
    }

    fn input(&mut self, button: MouseButton, state: ElementState) {
        match button {
            MouseButton::Left => self.left_button_down = state == ElementState::Pressed,
            MouseButton::Middle => self.middle_button_down = state == ElementState::Pressed,
            MouseButton::Right => self.right_button_down = state == ElementState::Pressed,
            _ => {},
        }
    }
}

struct ViewerApp {
    client_rect: Vec2,
    camera: OrbitCamera,
    depth_texture: DepthTexture,
    text_system: TextSystem,
    fps_counter: FPSCounter,
    line_drawer: EdgeDrawer,
    surface_drawer: SurfaceDrawer,
    smaa_target: SmaaTarget,
    rendered_edges: RenderedLine,
    cad_mesh: CadMesh,
    mouse_state: MouseState,
    wasm_engine: Option<WasmEngine>,
}

#[derive(Parser, Debug, Clone)]
struct AppArgs {
    #[arg(long, group = "model")]
    step_file: Option<PathBuf>,

    #[arg(long, group = "model")]
    kicad_file: Option<PathBuf>,

    #[arg(long, value_enum, group = "model")]
    example: Option<examples::Example>,

    #[arg(long, value_enum, group = "model")]
    wasm_path: Option<PathBuf>,
}

fn get_shape_mesh(shape: &Shape, graphics_device: &GraphicsDevice) -> CadMesh {
    let mesh = shape.mesh().expect("example shape should yield a valid triangulation");
    CadMesh::from_mesh(&mesh, graphics_device.device())
}

fn get_shape_edges(shape: &Shape, graphics_device: &GraphicsDevice) -> RenderedLine {
    // Pre-render the model edges.
    let line_thickness = 3.0;
    let mut line_builder = LineBuilder::new();

    for edge in shape.edges() {
        let mut segments = vec![];

        let mut last_point: Option<DVec3> = None;
        let mut length_so_far = 0.0;

        for point in edge.approximation_segments() {
            if let Some(last_point) = last_point {
                let point_a_length = length_so_far;
                length_so_far += (point - last_point).length();
                let point_b_length = length_so_far;

                segments.push(SegmentInstance {
                    pos_a: vec4(
                        last_point.x as f32,
                        last_point.y as f32,
                        last_point.z as f32,
                        line_thickness,
                    ),
                    pos_b: vec4(point.x as f32, point.y as f32, point.z as f32, line_thickness),
                    lengths: vec4(point_a_length as f32, point_b_length as f32, 0.0, 0.0),
                });
            }

            last_point = Some(point);
        }

        line_builder.add_round_line_strip(&segments);
    }

    line_builder.build(graphics_device.device())
}

impl GameApp for ViewerApp {
    fn window_title() -> &'static str {
        "Viewer"
    }

    fn init(graphics_device: &mut GraphicsDevice) -> Self {
        let args = AppArgs::parse();

        let mut wasm_engine = None;

        let shape = if let Some(step_file) = args.step_file {
            let bytes = std::fs::read(step_file).expect("Failed to read STEP file, {step_file}");
            // let s = Shape::read_step(step_file).expect("Failed to read STEP file, {step_file}");
            let s =
                Shape::read_step_from_bytes(&bytes).expect("Failed to read STEP file, {step_file}");
            let faces = s.faces();

            // println!("=== STEP File Information ===");

            // Face情報の表示
            println!("\n--- Face Information ---");
            for (i, face) in faces.enumerate() {
                println!("\nFace #{}", i + 1);
                println!("  Surface Type: {}", face.surface_type());
                println!("  Orientation: {:?}", face.orientation());

                // Surface詳細情報を表示
                match face.surface_details() {
                    opencascade::primitives::SurfaceDetails::Plane(plane) => {
                        println!("  Plane Details:");
                        println!("    Location: {:?}", plane.location);
                        println!("    Axis Location: {:?}", plane.axis_location);
                        println!("    Axis Direction (Normal): {:?}", plane.axis_direction);
                    },
                    opencascade::primitives::SurfaceDetails::Cylinder(cylinder) => {
                        println!("  Cylinder Details:");
                        println!("    Location: {:?}", cylinder.location);
                        println!("    Axis Location: {:?}", cylinder.axis_location);
                        println!("    Axis Direction: {:?}", cylinder.axis_direction);
                        println!("    Radius: {:.6}", cylinder.radius);
                    },
                    opencascade::primitives::SurfaceDetails::Cone(cone) => {
                        println!("  Cone Details:");
                        println!("    Location: {:?}", cone.location);
                        println!("    Axis Location: {:?}", cone.axis_location);
                        println!("    Axis Direction: {:?}", cone.axis_direction);
                        println!("    Reference Radius: {:.6}", cone.ref_radius);
                        println!("    Semi-Angle (radians): {:.6}", cone.semi_angle);
                        println!("    Semi-Angle (degrees): {:.2}°", cone.semi_angle.to_degrees());
                    },
                    opencascade::primitives::SurfaceDetails::Sphere(sphere) => {
                        println!("  Sphere Details:");
                        println!("    Location (Center): {:?}", sphere.location);
                        println!("    Axis Location: {:?}", sphere.axis_location);
                        println!("    Axis Direction: {:?}", sphere.axis_direction);
                        println!("    Radius: {:.6}", sphere.radius);
                    },
                    opencascade::primitives::SurfaceDetails::Torus(torus) => {
                        println!("  Torus Details:");
                        println!("    Location: {:?}", torus.location);
                        println!("    Axis Location: {:?}", torus.axis_location);
                        println!("    Axis Direction: {:?}", torus.axis_direction);
                        println!("    Major Radius: {:.6}", torus.major_radius);
                        println!("    Minor Radius: {:.6}", torus.minor_radius);
                    },
                    opencascade::primitives::SurfaceDetails::BSpline(bspline) => {
                        println!("  BSpline Surface Details:");
                        println!("    U Direction:");
                        println!("      Control Points: {}", bspline.u_profile.nb_poles);
                        println!("      Degree: {}", bspline.u_profile.degree);
                        println!("      Rational: {}", bspline.u_profile.is_rational);
                        println!("      Periodic: {}", bspline.u_profile.is_periodic);

                        let u_knot_count = bspline.u_profile.knots.len();
                        let u_mult_sum: i32 = bspline.u_profile.multiplicities.iter().sum();
                        println!(
                            "      {} unique knots, sum(multiplicities) = {}",
                            u_knot_count, u_mult_sum
                        );

                        println!("    V Direction:");
                        println!("      Control Points: {}", bspline.v_profile.nb_poles);
                        println!("      Degree: {}", bspline.v_profile.degree);
                        println!("      Rational: {}", bspline.v_profile.is_rational);
                        println!("      Periodic: {}", bspline.v_profile.is_periodic);

                        let v_knot_count = bspline.v_profile.knots.len();
                        let v_mult_sum: i32 = bspline.v_profile.multiplicities.iter().sum();
                        println!(
                            "      {} unique knots, sum(multiplicities) = {}",
                            v_knot_count, v_mult_sum
                        );

                        println!("    Poles ({}x{} grid): {:?}",
                            bspline.poles.len(),
                            bspline.poles.first().map(|v| v.len()).unwrap_or(0),
                            bspline.poles
                        );
                    },
                    opencascade::primitives::SurfaceDetails::Bezier(bezier) => {
                        println!("  Bezier Surface Details:");
                        println!("    U Direction:");
                        println!("      Control Points: {}", bezier.u_profile.nb_poles);
                        println!("      Degree: {}", bezier.u_profile.degree);
                        println!("    V Direction:");
                        println!("      Control Points: {}", bezier.v_profile.nb_poles);
                        println!("      Degree: {}", bezier.v_profile.degree);
                        println!("    Poles ({}x{} grid): {:?}",
                            bezier.poles.len(),
                            bezier.poles.first().map(|v| v.len()).unwrap_or(0),
                            bezier.poles
                        );
                    },
                    opencascade::primitives::SurfaceDetails::Unknown(_type_name) => {
                        // println!("  Details: Not available for {}", type_name);
                    },
                }

                let edge_count = face.edges().count();
                println!("  Number of Edges: {}", edge_count);

                face.wires().for_each(|wire| {
                    println!("  Wire: {:?}", wire.edges().count());
                    wire.edges().for_each(|edge| {});
                });
            }
            println!("Total Faces: {}\n", s.faces().count());

            // Edge情報の表示
            println!("--- Edge Information ---");
            let edges = s.edges();
            for (i, edge) in edges.enumerate() {
                println!("\nEdge #{}", i + 1);
                println!("  Curve Type: {}", edge.curve_type());

                // Curve詳細情報を表示
                match edge.curve_details() {
                    opencascade::primitives::CurveDetails::Line(line) => {
                        println!("  Line Details:");
                        println!("    Origin: {:?}", line.origin);
                        println!("    Direction: {:?}", line.direction);
                    },
                    opencascade::primitives::CurveDetails::Circle(circle) => {
                        println!("  Circle Details:");
                        println!("    Center: {:?}", circle.center);
                        println!("    Axis: {:?}", circle.axis);
                        println!("    Radius: {:.6}", circle.radius);
                    },
                    opencascade::primitives::CurveDetails::Ellipse(ellipse) => {
                        println!("  Ellipse Details:");
                        println!("    Center: {:?}", ellipse.center);
                        println!("    Axis: {:?}", ellipse.axis);
                        println!("    Major Radius: {:.6}", ellipse.major_radius);
                        println!("    Minor Radius: {:.6}", ellipse.minor_radius);
                    },
                    opencascade::primitives::CurveDetails::BSplineCurve(bspline) => {
                        println!("  BSpline Curve Details:");
                        println!("    Control Points: {}", bspline.profile.nb_poles);
                        println!("    Degree: {}", bspline.profile.degree);
                        println!("    Rational: {}", bspline.profile.is_rational);
                        println!("    Periodic: {}", bspline.profile.is_periodic);
                        println!("    Poles: {:?}", bspline.poles);

                        let knot_count = bspline.profile.knots.len();
                        let mult_sum: i32 = bspline.profile.multiplicities.iter().sum();
                        println!(
                            "    {} unique knots, sum(multiplicities) = {}",
                            knot_count, mult_sum
                        );
                    },
                    opencascade::primitives::CurveDetails::BezierCurve(bezier) => {
                        println!("  Bezier Curve Details:");
                        println!("    Control Points: {}", bezier.profile.nb_poles);
                        println!("    Degree: {}", bezier.profile.degree);
                        println!("    Poles: {:?}", bezier.poles);
                    },
                    opencascade::primitives::CurveDetails::Hyperbola(hyperbola) => {
                        println!("  Hyperbola Details:");
                        println!("    Center: {:?}", hyperbola.center);
                        println!("    Axis: {:?}", hyperbola.axis);
                        println!("    Major Radius: {:.6}", hyperbola.major_radius);
                        println!("    Minor Radius: {:.6}", hyperbola.minor_radius);
                    },
                    opencascade::primitives::CurveDetails::Parabola(parabola) => {
                        println!("  Parabola Details:");
                        println!("    Vertex: {:?}", parabola.vertex);
                        println!("    Axis: {:?}", parabola.axis);
                        println!("    Focal: {:.6}", parabola.focal);
                    },
                    opencascade::primitives::CurveDetails::OffsetCurve(offset_curve) => {
                        println!("  Offset Curve Details:");
                        println!("    Basis Curve Type: {}", offset_curve.basis_curve_type);
                        println!("    Offset: {:.6}", offset_curve.offset);
                    },
                    opencascade::primitives::CurveDetails::TrimmedCurve(trimmed_curve) => {
                        println!("  Trimmed Curve Details:");
                        println!("    Basis Curve Type: {}", trimmed_curve.basis_curve_type);
                        println!("    First Parameter: {:.6}", trimmed_curve.first_parameter);
                        println!("    Last Parameter: {:.6}", trimmed_curve.last_parameter);
                    },
                    opencascade::primitives::CurveDetails::Unknown(type_name) => {
                        println!("  Details: Not available for {}", type_name);
                    },
                }
            }
            // println!("Total Edges: {}\n", s.edges().count());
            // println!("==============================\n");

            s
        } else if let Some(kicad_file) = args.kicad_file {
            // Parse the kicad file, turn it into a face, extrude it by 1.6mm
            let pcb =
                KicadPcb::from_file(kicad_file).expect("Failed to read KiCAD PCB file, kicad_file");

            // Temporary - Unions all edges together to display without connecting them.
            let edges = pcb.layer_edges(&BoardLayer::EdgeCuts);
            edges.map(|edge| edge.into_shape()).reduce(|acc, edge| acc.union(&edge).into()).unwrap()

            // pcb.edge_cuts().to_face().extrude(glam::dvec3(0.0, 0.0, 1.6)).into()
        } else if let Some(example) = args.example {
            example.shape()
        } else if let Some(wasm_path) = args.wasm_path {
            let engine = WasmEngine::new(wasm_path);
            let shape = engine.shape();
            wasm_engine = Some(engine);

            match shape {
                Ok(shape) => shape,
                Err(err) => {
                    eprintln!("Error occurred while executing the model code: {err} - starting with a default cube.");
                    Shape::cube_centered(50.0)
                },
            }
        } else {
            eprintln!("Warning - no example or STEP file specified, you get a default cube.");
            Shape::cube_centered(50.0)
        };

        let cad_mesh = get_shape_mesh(&shape, graphics_device);
        let rendered_edges = get_shape_edges(&shape, graphics_device);

        // Create SMAA target
        let (width, height) = graphics_device.surface_dimensions();
        let device = graphics_device.device();
        let queue = graphics_device.queue();
        let swapchain_format = graphics_device.surface_config().format;

        let smaa_target =
            SmaaTarget::new(device, queue, width, height, swapchain_format, SmaaMode::Smaa1X);

        let surface_texture_format = graphics_device.surface_texture_format();

        let depth_texture = DepthTexture::new(device, width, height);
        let depth_texture_format = depth_texture.format();

        Self {
            client_rect: vec2(width as f32, height as f32),
            camera: OrbitCamera::new(width, height, Vec3::new(40.0, -40.0, 20.0)),
            depth_texture,
            text_system: TextSystem::new(
                device,
                surface_texture_format,
                Some(depth_texture_format),
                width,
                height,
            ),
            fps_counter: FPSCounter::new(),
            line_drawer: EdgeDrawer::new(
                device,
                surface_texture_format,
                depth_texture_format,
                width,
                height,
            ),
            surface_drawer: SurfaceDrawer::new(
                device,
                surface_texture_format,
                depth_texture_format,
            ),
            smaa_target,
            cad_mesh,
            rendered_edges,
            mouse_state: Default::default(),
            wasm_engine,
        }
    }

    fn resize(&mut self, graphics_device: &mut GraphicsDevice, width: u32, height: u32) {
        self.client_rect = vec2(width as f32, height as f32);
        self.camera.resize(width, height);
        self.depth_texture = DepthTexture::new(graphics_device.device(), width, height);
        self.text_system.resize(width, height);
        self.line_drawer.resize(width, height);
        self.smaa_target.resize(graphics_device.device(), width, height);
    }

    fn handle_window_event(&mut self, event: &WindowEvent, window_target: &ActiveEventLoop) {
        let screen_diagonal = self.client_rect.length();

        match event {
            WindowEvent::RotationGesture { delta, .. } => {
                let axis = Vec3::new(0.0, 0.0, 1.0);
                let rotator = Quat::from_axis_angle(axis, TOUCHPAD_ROTATE_MULTIPLIER * delta);
                self.camera.rotate(rotator);
            },
            WindowEvent::CursorMoved { position, .. } => {
                let delta = self.mouse_state.delta(*position);
                // On the screen, Y is DOWN, but in camera space, it's UP
                let camera_space_delta =
                    Vec2::new(delta.0 as f32, -delta.1 as f32) / screen_diagonal;
                if self.mouse_state.left_button_down {
                    // Construct the camera space rotation axis perpendicular to delta
                    let axis = Vec3::new(camera_space_delta.y, -camera_space_delta.x, 0.0);
                    let magnitude = axis.length();
                    if magnitude > 0.0 {
                        let rotator =
                            Quat::from_axis_angle(axis.normalize(), ROTATE_MULTIPLIER * magnitude);
                        self.camera.rotate(rotator);
                    }
                }
                if self.mouse_state.middle_button_down {
                    self.camera.pan(PAN_MULTIPLIER * camera_space_delta);
                }
                if self.mouse_state.right_button_down {
                    self.camera.zoom(camera_space_delta.y * ZOOM_MULTIPLIER);
                }
            },
            WindowEvent::MouseInput { state, button, .. } => {
                self.mouse_state.input(*button, *state)
            },
            WindowEvent::MouseWheel { delta: PixelDelta(delta), .. } => {
                // winit can not distinguish mouse wheel and touchpad pan events unfortunately.
                // Because of that, we assign pan operation to MouseWheel events. For mice, you
                // need to instead use mouse move while holding down the right button.

                // On the screen, Y is DOWN, but in camera space, it's UP
                let camera_space_delta =
                    Vec2::new(delta.x as f32, -delta.y as f32) / screen_diagonal;

                self.camera.pan(TOUCHPAD_PAN_MULTIPLIER * camera_space_delta);
            },
            WindowEvent::PinchGesture { delta, .. } => {
                let zoom_delta = *delta as f32 * TOUCHPAD_ZOOM_MULTIPLIER;
                self.camera.zoom(-zoom_delta);
            },
            WindowEvent::KeyboardInput {
                event:
                    KeyEvent {
                        physical_key: PhysicalKey::Code(key_code),
                        state: ElementState::Pressed,
                        ..
                    },
                ..
            } => match key_code {
                KeyCode::Escape => window_target.exit(),
                KeyCode::KeyP => self.camera.use_perspective(),
                KeyCode::KeyO => self.camera.use_orthographic(),
                KeyCode::KeyX => self.line_drawer.toggle_back_edge_drawing(),
                _ => {},
            },
            _ => {},
        }
    }

    fn tick(&mut self, _dt: f32) {}

    fn render(&mut self, graphics_device: &mut GraphicsDevice, _window: &Window) {
        if let Some(wasm_engine) = &mut self.wasm_engine {
            let start = std::time::Instant::now();
            match wasm_engine.new_shape_if_wasm_changed() {
                Some(Ok(new_shape)) => {
                    self.cad_mesh = get_shape_mesh(&new_shape, graphics_device);
                    self.rendered_edges = get_shape_edges(&new_shape, graphics_device);
                    println!("Created a new shape in {:?}", start.elapsed());
                },
                Some(Err(err)) => {
                    println!("Error occurred while executing the model code: {err}");
                },
                _ => {},
            }
        }

        let mut frame_encoder = graphics_device.begin_frame();

        let smaa_render_target = self.smaa_target.start_frame(
            graphics_device.device(),
            graphics_device.queue(),
            &frame_encoder.backbuffer_view,
        );

        let camera_matrix = self.camera.matrix();
        let transform = Mat4::IDENTITY;

        let mut render_pass =
            frame_encoder.encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Model render pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &smaa_render_target,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color { r: 0.3, g: 0.3, b: 0.3, a: 1.0 }),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &self.depth_texture.view,
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                occlusion_query_set: None,
                timestamp_writes: None,
            });

        self.surface_drawer.render(
            &mut render_pass,
            graphics_device.queue(),
            &self.cad_mesh,
            camera_matrix,
            transform,
        );

        let dash_size = 0.5;
        let gap_size = 0.5;

        self.line_drawer.draw(
            &self.rendered_edges,
            &mut render_pass,
            graphics_device.queue(),
            camera_matrix,
            transform,
            dash_size,
            gap_size,
        );

        self.text_system.render_horizontal(
            TextAlignment {
                x: AxisAlign::Start(10),
                y: AxisAlign::Start(10),
                max_width: None,
                max_height: None,
            },
            &[StyledText::default_styling(&format!("FPS: {}", self.fps_counter.fps()))],
            &mut render_pass,
            graphics_device.queue(),
        );

        drop(render_pass);

        graphics_device.queue().submit(Some(frame_encoder.encoder.finish()));

        smaa_render_target.resolve();
        frame_encoder.frame.present();

        self.fps_counter.tick();
    }
}

fn main() -> Result<(), Error> {
    simple_game::run_game_app::<ViewerApp>()?;
    Ok(())
}
