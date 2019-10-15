/*! Managing Wayland outputs */

use std::vec::Vec;


/// Gathers stuff defined in C or called by C
pub mod c {
    use super::*;
    
    use std::os::raw::{ c_char, c_void };

    use ::util::c::COpaquePtr;

    // Defined in C

    #[repr(transparent)]
    #[derive(Clone, PartialEq)]
    pub struct WlOutput(*const c_void);

    #[repr(C)]
    struct WlOutputListener<T: COpaquePtr> {
        geometry: extern fn(
            T, // data
            WlOutput,
            i32, // x
            i32, // y
            i32, // physical_width
            i32, // physical_height
            i32, // subpixel
            *const c_char, // make
            *const c_char, // model
            i32, // transform
        ),
        mode: extern fn(
            T, // data
            WlOutput,
            u32, // flags
            i32, // width
            i32, // height
            i32, // refresh
        ),
        done: extern fn(
            T, // data
            WlOutput,
        ),
        scale: extern fn(
            T, // data
            WlOutput,
            i32, // factor
        ),
    }
    
    bitflags!{
        /// Map to `wl_output.mode` values
        pub struct Mode: u32 {
            const NONE = 0x0;
            const CURRENT = 0x1;
            const PREFERRED = 0x2;
        }
    }

    extern "C" {
        // Rustc wrongly assumes
        // that COutputs allows C direct access to the underlying RefCell
        #[allow(improper_ctypes)]
        fn squeek_output_add_listener(
            wl_output: WlOutput,
            listener: *const WlOutputListener<COutputs>,
            data: COutputs,
        ) -> i32;
    }

    type COutputs = ::util::c::Wrapped<Outputs>;

    // Defined in Rust

    extern fn outputs_handle_geometry(
        _outputs: COutputs,
        _wl_output: WlOutput,
        _x: i32, _y: i32,
        _phys_width: i32, _phys_height: i32,
        _subpixel: i32,
        _make: *const c_char, _model: *const c_char,
        _transform: i32,
    ) {
        //println!("geometry handled {},{}", x, y);
        // Totally unused
    }

    extern fn outputs_handle_mode(
        outputs: COutputs,
        wl_output: WlOutput,
        flags: u32,
        width: i32,
        height: i32,
        _refresh: i32,
    ) {
        let flags = Mode::from_bits(flags).unwrap_or_else(|| {
            eprintln!("Warning: received invalid wl_output.mode flags");
            Mode::NONE
        });
        let outputs = outputs.clone_ref();
        let mut outputs = outputs.borrow_mut();
        let mut output_state: Option<&mut OutputState> = outputs.outputs
            .iter_mut()
            .find_map(|o|
                if o.output == wl_output { Some(&mut o.pending) } else { None }
            );
        match output_state {
            Some(ref mut state) => {
                if flags.contains(Mode::CURRENT) {
                    state.current_mode = Some(super::Mode { width, height});
                }
            },
            None => eprintln!("Wayland error: Got mode on unknown output"),
        };
    }

    extern fn outputs_handle_done(
        outputs: COutputs,
        wl_output: WlOutput,
    ) {
        let outputs = outputs.clone_ref();
        let mut outputs = outputs.borrow_mut();
        let mut output = outputs.outputs
            .iter_mut()
            .find(|o| o.output == wl_output);
        match output {
            Some(ref mut output) => { output.current = output.pending.clone(); }
            None => eprintln!("Wayland error: Got done on unknown output"),
        };
    }

    extern fn outputs_handle_scale(
        outputs: COutputs,
        wl_output: WlOutput,
        factor: i32,
    ) {
        let outputs = outputs.clone_ref();
        let mut outputs = outputs.borrow_mut();
        let output_state = outputs.outputs
            .iter_mut()
            .find_map(|o|
                if o.output == wl_output { Some(&mut o.pending) } else { None }
            );
        match output_state {
            Some(state) => { state.scale = factor; }
            None => eprintln!("Wayland error: Got done on unknown output"),
        };
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_outputs_new() -> COutputs {
        COutputs::new(Outputs { outputs: Vec::new() })
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_outputs_free(outputs: COutputs) {
        unsafe { outputs.unwrap() }; // gets dropped
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_outputs_register(raw_collection: COutputs, output: WlOutput) {
        let collection = raw_collection.clone_ref();
        let mut collection = collection.borrow_mut();
        collection.outputs.push(Output {
            output: output.clone(),
            pending: OutputState::uninitialized(),
            current: OutputState::uninitialized(),
        });

        unsafe { squeek_output_add_listener(
            output,
            &WlOutputListener {
                geometry: outputs_handle_geometry,
                mode: outputs_handle_mode,
                done: outputs_handle_done,
                scale: outputs_handle_scale,
            } as *const WlOutputListener<COutputs>,
            raw_collection,
        )};
    }
    
    #[no_mangle]
    pub extern "C"
    fn squeek_outputs_get_current(raw_collection: COutputs) -> WlOutput {
        let collection = raw_collection.clone_ref();
        let collection = collection.borrow();
        collection.outputs[0].output.clone()
    }

    #[no_mangle]
    pub extern "C"
    fn squeek_outputs_get_perceptual_width(
        raw_collection: COutputs,
        wl_output: WlOutput,
    ) -> i32 {
        let collection = raw_collection.clone_ref();
        let collection = collection.borrow();

        let output_state = collection.outputs
            .iter()
            .find_map(|o|
                if o.output == wl_output { Some(&o.current) } else { None }
            );

        match output_state {
            Some(OutputState {
                current_mode: Some(super::Mode { width, height: _ } ),
                scale,
            }) => width / scale,
            _ => {
                eprintln!("No width registered on output");
                0
            },
        }
    }
    // TODO: handle unregistration
}

#[derive(Clone)]
struct Mode {
    width: i32,
    height: i32,
}

#[derive(Clone)]
pub struct OutputState {
    current_mode: Option<Mode>,
    scale: i32,
}

impl OutputState {
    fn uninitialized() -> OutputState {
        OutputState {
            current_mode: None,
            scale: 1,
        }
    }
}

pub struct Output {
    output: c::WlOutput,
    pending: OutputState,
    current: OutputState,
}

pub struct Outputs {
    outputs: Vec<Output>,
}
