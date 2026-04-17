//! Limine boot protocol request statics. Limine scans for these specific
//! magic numbers in the kernel image.

use limine::request::{
    FramebufferRequest, HhdmRequest, MemoryMapRequest, MpRequest, RequestsEndMarker,
    RequestsStartMarker, RsdpRequest, StackSizeRequest,
};
use limine::BaseRevision;

#[used]
#[link_section = ".requests_start_marker"]
pub static REQUESTS_START: RequestsStartMarker = RequestsStartMarker::new();

#[used]
#[link_section = ".requests"]
pub static BASE_REVISION: BaseRevision = BaseRevision::with_revision(2);

#[used]
#[link_section = ".requests"]
pub static HHDM_REQUEST: HhdmRequest = HhdmRequest::new();

#[used]
#[link_section = ".requests"]
pub static MEMMAP_REQUEST: MemoryMapRequest = MemoryMapRequest::new();

#[used]
#[link_section = ".requests"]
pub static FRAMEBUFFER_REQUEST: FramebufferRequest = FramebufferRequest::new();

#[used]
#[link_section = ".requests"]
pub static MP_REQUEST: MpRequest = MpRequest::new();

#[used]
#[link_section = ".requests"]
pub static RSDP_REQUEST: RsdpRequest = RsdpRequest::new();

#[used]
#[link_section = ".requests"]
pub static STACK_REQUEST: StackSizeRequest = StackSizeRequest::new().with_size(128 * 1024);

#[used]
#[link_section = ".requests_end_marker"]
pub static REQUESTS_END: RequestsEndMarker = RequestsEndMarker::new();
