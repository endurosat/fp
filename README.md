# EnduroSat Function Protocol (FP) Service Implementation

[![License](https://img.shields.io/badge/license-EnduroSat%20Software%20License-blue.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Multi--platform-green.svg)](#-system-requirements)

This repository contains the core implementation of the **EnduroSat Function Protocol** (FP) service, which EnduroSat hardware modules use to communicate with each other and with the ground segment. The FP service provides a robust, session-aware communication framework with advanced features like ARM-and-FIRE operations for critical commands.

## 🚀 Overview

FP is used together with code generated from Franca IDL (FIDL) interface definitions by EnduroSat's macchiato code generator. macchiato is available to EnduroSat customers on request from [EnduroSat support](https://www.endurosat.com/contact/).

The FP service provides:

* **🔄 SerDes mechanism** for user-defined APIs with automatic code generation
* **📡 FP-specific session handling** with timeout management and tracking
* **🔒 ARM-and-FIRE mechanism** for critical operations requiring confirmation
* **🌐 Multi-transport support** (CSP, RS485, local communication)
* **🛡️ Thread-safe operation** with configurable locking mechanisms
* **⚡ Zero-copy operation** for optimal performance
* **📊 Comprehensive error handling** and status reporting
* **🔍 Built-in diagnostics** and session metrics

## ✨ Key Features

### 🔒 ARM-and-FIRE Mechanism
The ARM-and-FIRE mechanism provides a two-stage execution process for critical operations:

- **Two-stage execution** prevents accidental command execution
- **Data integrity validation** using hash comparison between ARM and FIRE stages
- **Automatic timeout** (15 minutes default) with session cleanup
- **Explicit confirmation** required before executing dangerous commands
- **Cancellation support** via DISARM requests

### 🔄 Advanced Session Management
- **Client sessions** for request/response tracking with timeout handling
- **Server sessions** for ARM-and-FIRE operations with state persistence
- **Automatic cleanup** of expired sessions to prevent resource leaks
- **Configurable session limits** and tracking metrics
- **Thread-safe session isolation** between different communication contexts

### 🚀 Multi-Transport Architecture
- **CSP (CubeSat Space Protocol)** integration for space communications
- **RS485** serial communication for ground equipment
- **Local** in-process communication for internal modules
- **Extensible architecture** for additional transport protocols
- **Context-specific configuration** for each transport type

### 🛡️ Thread Safety & Performance
- **Context-specific locking** mechanisms to minimize contention
- **Registry protection** for FIDL interface management
- **Zero-copy operation** for communication buffers
- **Efficient session tracking** with minimal overhead
- **Lock-free operations** where possible

## 📋 System Requirements

The implementation is designed to integrate with host systems that provide:

### Compiler Support
- **C11-compliant compiler** (GCC, MSVC, TI MSP430)
- **Standard library** support for basic types and functions
- **Packed structure** support for protocol definitions

### Runtime Environment
- **Multi-threaded environment** (RTOS or OS-hosted preferred)
- **Timer services** for timeout management and session tracking
- **Mutex/locking primitives** for thread synchronization
- **Event signaling** mechanisms for synchronous operations

### Platform Services
- **Hash calculation** capabilities (CRC16 or similar algorithm)
- **Memory management** for session storage
- **Trace/logging** infrastructure (optional but recommended)

All external interfaces are declared in the `config/fp_arch.h.template` header and must be implemented by the host system. Template implementations are provided for common scenarios.

## 📁 Repository Structure

```
fp/
├── 📁 inc/                    # Public interface headers
│   ├── fp_disp.h              # Main dispatcher interface
│   ├── fp_disp_client.h       # Client-side operations
│   ├── fp_disp_server.h       # Server-side operations
│   ├── fp_disp_session_mgr.h  # Session management
│   ├── fp_registry.h          # FIDL interface registry
│   ├── fp_proto.h             # Protocol definitions and utilities
│   └── fp_defs.h              # Common type definitions
├── 📁 src/                    # Implementation files
│   ├── fp_disp.c              # Core dispatcher logic
│   ├── fp_disp_client.c       # Client request handling
│   ├── fp_disp_server.c       # Server request processing
│   ├── fp_disp_session_mgr.c  # Session lifecycle management
│   ├── fp_registry.c          # FIDL registry implementation
│   ├── fp_proto.c             # Protocol utilities
│   └── fp_internal.c          # Internal helper functions
├── 📁 config/                 # Configuration templates
│   ├── fp_arch.h.template     # Architecture interface template
│   ├── fp_config.h.template   # System configuration template
│   ├── fp_config_user.h.template # User configuration header
│   └── fp_config_user.c.template # User configuration implementation
├── 📁 test/                   # Unit tests and test utilities
├── 📁 diagrams/               # PlantUML documentation diagrams
│   ├── fp_regular_dispatch.puml    # Regular request/response flow
│   ├── fp_arm_and_fire.puml        # ARM-and-FIRE sequence
│   ├── fp_system_architecture.puml # System architecture overview
│   ├── fp_session_lifecycle.puml   # Session management lifecycle
│   ├── fp_error_handling.puml      # Error scenarios and handling
│   ├── fp_message_format.puml      # Protocol message structure
│   ├── fp_transport_integration.puml # Transport layer patterns
│   ├── fp_session_class_hierarchy.puml # FP session class hierarchy
│   ├── generate_svg.sh             # Diagram generation script
│   └── 📁 svg/                     # Generated SVG diagrams
├── LICENSE                    # EnduroSat Software License
├── LICENSES/                  # License texts referenced by SPDX headers
├── CONTRIBUTING.md            # How to contribute
└── README.md                  # This comprehensive documentation
```

## 🏗️ Architecture Overview

The FP service uses a context-driven architecture to eliminate global state dependencies and enable multi-transport operation. The only global component is the FIDL registry, which maintains a singleton pattern for interface definitions shared across all FP-bound interfaces.

### Core Components

1. **Dispatcher (`fp_disp`)** - Central routing and frame processing
2. **Session Manager (`fp_disp_session_mgr`)** - Lifecycle and timeout management
3. **Registry (`fp_registry`)** - FIDL interface registration and lookup
4. **Protocol Handler (`fp_proto`)** - Message format and attribute processing
5. **Transport Abstraction** - Pluggable communication layer support

### Communication Flow

The service operates on a zero-copy principle where communication buffers provided to `fp_disp_execute` are transparently passed to application handlers. Data copying only occurs for synchronous FP requests where responses arrive in different thread contexts.

## 📨 Message Format

### FP Header Structure
The FP message header (`fp_msg_hdr_t`) is a packed structure that contains the following fields:

```c
PACK_STRUCT_BEGIN
typedef struct
{
    uint16_t iface_id; /**< Interface ID (matching the FIDL interface ID) */
    uint32_t func_id;  /**< Function ID with embedded attribute bits */
    uint16_t cmd_id;   /**< Command ID (unique ID used to match requests and responses) */
    uint8_t status;    /**< Status code (used by responses; requests set to 0) */
} fp_msg_hdr_t;
PACK_STRUCT_END
```

**Field Descriptions:**
- **`iface_id`**: Identifies the FIDL interface (0-65535)
- **`func_id`**: Contains both function ID and attribute bits (see below)
- **`cmd_id`**: Unique identifier for request/response matching
- **`status`**: Operation result code (see [Status Codes](#-status-codes))

### Function ID Format
The `func_id` field uses a 32-bit value with embedded attribute bits:

```
         31            24      20                                      0
         |             |       |                                       |
MS bit [ x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x ] LS bit 0 (32 bits)
         ----------------------- ---------------------------------------
                    |                                     |
         Function Attribute Bits (12 bits)            Function ID (20 bits)
```

**Bit Layout:**
- **Bits 0-19**: Actual function ID (1,048,576 possible functions)
- **Bits 20-31**: Attribute bits for frame type and special operations

### Function Attribute Bits
The upper bits of the `func_id` field contain special attribute bits:

| Bit Position | Mask Value   | Constant Name | Description |
|--------------|-------------:|---------------|-------------|
| 31           | 0x80000000   | `RESP_FUNC_ID_MASK` | Response frame indicator |
| 30           | 0x40000000   | `ARMREQ_FUNC_ID_MASK` | ARM request frame |
| 29           | 0x20000000   | `FIREREQ_FUNC_ID_MASK` | FIRE request frame |
| 28           | 0x10000000   | `DISARMREQ_FUNC_ID_MASK` | DISARM request frame |
| 27           | 0x08000000   | `BROADCAST_FUNC_ID_MASK` | Broadcast frame indicator |
| 26-20        | 0x07F00000   | Reserved | Future use |

## 🏷️ Frame Types
The protocol supports different frame types defined in `fp_frame_type_t`:

| Frame Type | Value | Constant | Description |
|------------|-------|----------|-------------|
| **REQ** | 0 | `FP_FRAME_TYPE_REQ` | Standard Function Protocol request |
| **REQ_ARM** | 1 | `FP_FRAME_TYPE_REQ_ARM` | ARM request frame (first stage) |
| **REQ_FIRE** | 2 | `FP_FRAME_TYPE_REQ_FIRE` | FIRE request frame (second stage) |
| **REQ_DISARM** | 3 | `FP_FRAME_TYPE_REQ_DISARM` | DISARM request frame (cancel) |
| **RESP** | 4 | `FP_FRAME_TYPE_RESP` | Standard Function Protocol response |
| **BROADCAST** | 5 | `FP_FRAME_TYPE_BROADCAST` | Broadcast frame (one-way notification) |

## 📊 Status Codes
Response messages use the `status` field to indicate the result of an operation:

> **Status code origin:** Not all status codes travel over the wire. There are two distinct kinds:
>
> * **Wire codes** — carried in the `status` field of actual FP frames exchanged between a server and a client. These include success codes (0x20-0x2F) and server error codes (0x50-0x5F). A server populates these values and a client reads them from the received frame.
>
> * **Internal codes** — generated locally by the FP dispatcher and never appear in any frame on the wire. These are used to communicate internal state back to the calling user implementation: client-side errors (0x10-0x1F, e.g. bad arguments or send failure), timeout/resource errors (0x30-0x3F), and protocol violation errors (0x40-0x4F, e.g. unexpected frame type or mismatched size). The dispatcher synthesises these values and delivers them via callback status or return value.

### Success Codes
| Status Code | Value | Constant | Description |
|-------------|-------|----------|-------------|
| **NO ERROR** | 0x20 | `FP_CMD_STATUS_NOERROR` | Operation executed successfully |

### Client Error Codes (0x10-0x1F)
| Status Code | Value | Constant | Description |
|-------------|-------|----------|-------------|
| **INVALID ARGS** | 0x10 | `FP_CMD_STATUS_INVARGS` | Argument validity check failed |
| **SEND ERROR** | 0x11 | `FP_CMD_STATUS_SENDERR` | Frame could not be sent |

### Timeout & Resource Codes (0x30-0x3F)
| Status Code | Value | Constant | Description |
|-------------|-------|----------|-------------|
| **TIMEOUT** | 0x30 | `FP_CMD_STATUS_TIMEOUT` | Server did not respond within timeout |
| **NO MEMORY** | 0x31 | `FP_CMD_STATUS_NO_MEM` | Not enough memory to track request |

### Protocol Violation Codes (0x40-0x4F)

| Status Code               | Value | Constant                       | Description                                                                     |
|---------------------------|-------|--------------------------------|---------------------------------------------------------------------------------|
| **UNEXPECTED FRAME TYPE** | 0x40  | `FP_CMD_STATUS_UNEXP_FRM_TYPE` | Response frame arrived with an unexpected frame type (e.g. not a RESP frame)    |
| **UNEXPECTED FRAME SIZE** | 0x41  | `FP_CMD_STATUS_UNEXP_FRM_SIZE` | Response frame arrived with a size that does not match the FIDL-declared size   |

These codes are generated by the FP dispatcher itself — they are never sent over the wire by a server. When a protocol violation is detected:

- **Async path**: the user callback is invoked with the violation status and `p_data == NULL`.
- **Sync path**: `fp_disp_client_send_req` returns the violation status and the caller's response buffer is zeroed.
  When the FIDL-declared response size is known (the registered-client route, `fp_disp_client_handle_resp`),
  the zeroing and the success copy never write more than that size into the caller buffer, even though the
  pool-wide slot capacity (`resp_data_max_size`) can be much larger. On the unmatched route
  (`fp_disp_client_handle_resp_unmatched`: any response whose interface has no registered client serdes) no
  typed size exists, so an unbounded caller is still cleared or copied up to `resp_data_max_size`. Callers
  that can land there must use `fp_disp_client_send_req_bounded`. The caller buffer is normally one typed
  response struct on the caller stack.

### Server Error Codes (0x50-0x5F)
| Status Code | Value | Constant | Description |
|-------------|-------|----------|-------------|
| **NOT SUPPORTED** | 0x50 | `FP_CMD_STATUS_FUNC_NOT_SUPPORTED` | Server doesn't support the operation |
| **ARMED** | 0x51 | `FP_CMD_STATUS_ARMED` | Request is armed but not yet executed |
| **DISARMED** | 0x52 | `FP_CMD_STATUS_DISARMED` | Request is disarmed |
| **ATTR ERROR** | 0x53 | `FP_CMD_STATUS_REQ_ATTR_ERR` | Request has unexpected attribute bits |
| **DATA INTEGRITY** | 0x54 | `FP_CMD_STATUS_DATA_INTEGRITY_ERR` | Data fields changed between ARM/FIRE |

**Status Code Categories:**
- **0x00-0x0F**: Reserved for future use
- **0x10-0x1F**: Client-side errors
- **0x20-0x2F**: Success codes
- **0x30-0x3F**: Timeout and resource errors
- **0x40-0x4F**: Protocol violation codes (internal, dispatcher-generated)
- **0x50-0x5F**: Server-side errors
- **0x60-0xFF**: Reserved for future use

## 🔒 ARM-and-FIRE Mechanism

The ARM-and-FIRE mechanism provides a robust two-stage triggering for critical operations that require explicit confirmation before execution.

### Protocol Flow

#### 1. ARM Stage
```
Client → Server: ARM request with operation data
Server → Client: ARMED status (if validation passes)
```

**Server Actions:**
- Validates the request parameters
- Calculates hash of request data using `fp_arch_calc_hash()`
- Creates server session to track the armed request
- Stores request hash in session for later verification
- Starts 15-minute timeout timer
- Responds with `FP_CMD_STATUS_ARMED`

#### 2. FIRE Stage
```
Client → Server: FIRE request with identical operation data
Server → Client: Operation result (if hash matches)
```

**Server Actions:**
- Finds existing server session for the request
- Calculates hash of new request data
- Compares new hash with stored hash
- **If hashes match**: Executes operation and closes session
- **If hashes differ**: Returns `FP_CMD_STATUS_DATA_INTEGRITY_ERR`

#### 3. DISARM Stage (Optional)
```
Client → Server: DISARM request
Server → Client: DISARMED status
```

**Server Actions:**
- Finds and closes the server session
- Stops the timeout timer
- Responds with `FP_CMD_STATUS_DISARMED`

### Timeout Handling

**Automatic Disarm:**
- Default timeout: **15 minutes** (`ARMED_REQ_TOUT_S`)
- Configurable via `fp_config.h`
- Session automatically closed on timeout
- No notification sent to client on timeout

**Session Tracking:**
```c
// Session tracking runs in a background thread
fp_disp_session_mgr_track_sessions(&sys_context);
```

### Data Integrity Protection

The ARM-and-FIRE mechanism ensures data integrity through:

1. **Hash Validation**: Request data must be identical between ARM and FIRE
2. **Session Isolation**: Each armed request has its own session
3. **Timeout Protection**: Prevents indefinite resource holding
4. **Explicit Confirmation**: Requires deliberate FIRE action

### Use Cases

**Critical Operations:**
- Satellite attitude changes
- Payload power control
- Communication system resets
- Firmware updates
- Mission-critical commands

**Benefits:**
- Prevents accidental execution
- Provides confirmation step
- Detects data corruption
- Enables operation cancellation

## 📡 Broadcast Frames

Broadcast frames provide a mechanism for **server-to-client one-way notifications** that do not require or expect a response.

### Characteristics

**One-Way Communication:**
- Broadcasts are sent from server to client only
- No response is expected or generated
- Client requests cannot use broadcast frame type (rejected with `FP_CMD_STATUS_INVARGS`)

**Routing Behavior:**
- Broadcasts route to **CLIENT-type FIDL handlers** (same routing as response frames)
- This reflects the semantic direction: server → client
- If no matching CLIENT-type handler is found, the broadcast is **silently ignored** (no error response sent)

**Command ID Generation:**
- Each broadcast generates a unique `cmd_id` using `fp_internal_gen_cmd_id()`
- Command IDs are generated atomically for thread safety
- Unlike request/response pairs, broadcast `cmd_id` values are not matched to any client session

### Sending Broadcasts (Server-Side)

Use `fp_disp_server_send_bcast()` to send broadcast notifications:

```c
bool fp_disp_server_send_bcast(fp_dispatch_info_t *p_disp_ctx,
                               const uint16_t      iface_id,
                               const uint32_t      func_id,
                               uint8_t *const      p_bcast,
                               const uint32_t      bcast_size);
```

**Parameters:**
- `p_disp_ctx`: Dispatch context containing system context and transport callbacks
- `iface_id`: FIDL interface ID for the broadcast
- `func_id`: Function ID (bare, without frame type bits)
- `p_bcast`: Pointer to broadcast message buffer (must be at least `sizeof(fp_msg_hdr_t)`)
- `bcast_size`: Size of broadcast message in bytes (minimum: `sizeof(fp_msg_hdr_t)` for zero-argument broadcasts)

**Returns:** `true` if broadcast was sent successfully, `false` otherwise

**Note:** Zero-argument broadcasts (broadcasts with no payload beyond the FP message header) are fully supported. For such broadcasts, `bcast_size` should be `sizeof(fp_msg_hdr_t)`.

### Receiving Broadcasts (Client-Side)

**FIDL Registration:**
- Register CLIENT-type FIDL interfaces to receive broadcasts
- The FP service library routes broadcasts through the same `fp_disp_execute()` mechanism used for responses
- Broadcast callbacks receive full `fp_dispatch_info_t` context

**API Usage:**

Use `fp_disp_client_handle_bcast()` to handle received broadcast frames:

```c
void fp_disp_client_handle_bcast(fp_dispatch_info_t            *p_disp_ctx,
                                 const uint32_t                 bcast_type_size,
                                 fp_generic_broadcast_handler_t p_bcast_handler);
```

This function:
- Validates the frame type is `FP_FRAME_TYPE_BROADCAST`
- Validates the broadcast size matches the expected type size
- Invokes the user-registered broadcast handler callback

**Note:** The `bcast_type_size` parameter specifies the expected size of the broadcast message. For zero-argument broadcasts (broadcasts with no payload beyond the header), this should be `sizeof(fp_msg_hdr_t)`.

**Generated Code Responsibilities:**

The code generation layer (macchiato) is responsible for:
- **Subscription Management**: Determining which broadcasts a client wants to receive
- **Callback Routing**: Mapping broadcast function IDs to user-registered callbacks via `fp_disp_client_handle_bcast()`
- **Static Storage**: Maintaining subscription state and callback registrations
- **Filtering**: Ignoring unwanted broadcasts at the application level
- **Type Safety**: Deserializing broadcast payloads to appropriate data structures

**Example:**

```c
// User-defined broadcast handler
void user_telemetry_handler(void *p_disp_ctx, uint8_t *p_bcast_data)
{
    telemetry_broadcast_t *p_telemetry = (telemetry_broadcast_t *)p_bcast_data;
    printf("Telemetry: temp=%d, voltage=%d\n",
           p_telemetry->temperature, p_telemetry->voltage);
}

// Generated FIDL callback for broadcast function
void fidl_telemetry_broadcast_callback(fp_dispatch_info_t *p_disp_ctx)
{
    // Use broadcast handler API
    fp_disp_client_handle_bcast(p_disp_ctx,
                                sizeof(telemetry_broadcast_t),
                                &user_telemetry_handler);
}

// FIDL registration
fidl_func_t telemetry_funcs[] = {
    { .func_id = TELEMETRY_BCAST_FUNC_ID,
      .p_on_func_cbk = &fidl_telemetry_broadcast_callback }
};

fidl_if_t telemetry_client_if = {
    .version = 1,
    .gen_type = FIDL_TYPE_CLIENT,
    .fidl_if_id = TELEMETRY_IFACE_ID,
    .func_count = 1,
    .p_func_list = telemetry_funcs
};

fp_registry_add_fidl_if(&telemetry_client_if);
```

**Zero-Argument Broadcast Example:**

```c
// User-defined handler for zero-argument broadcast (e.g., heartbeat)
void user_heartbeat_handler(void *p_disp_ctx, uint8_t *p_bcast_data)
{
    // No payload to process - just the notification itself is meaningful
    fp_msg_hdr_t *p_hdr = (fp_msg_hdr_t *)p_bcast_data;
    printf("Heartbeat received from iface_id=%u\n", p_hdr->iface_id);
}

// Generated FIDL callback for zero-argument broadcast
void fidl_heartbeat_broadcast_callback(fp_dispatch_info_t *p_disp_ctx)
{
    // For zero-argument broadcasts, size is just the header
    fp_disp_client_handle_bcast(p_disp_ctx,
                                sizeof(fp_msg_hdr_t),
                                &user_heartbeat_handler);
}
```

### Use Cases

**Typical Broadcast Scenarios:**
- Telemetry updates (periodic sensor data)
- System status notifications (mode changes, warnings)
- Event announcements (threshold crossings, state transitions)
- Heartbeat/keepalive messages
- Multi-client notifications (same broadcast to multiple clients)

**When to Use Broadcasts vs. Responses:**
- Use **broadcasts** for unsolicited notifications that don't correspond to a specific client request
- Use **responses** for replies to client requests where the client expects a response

## 🔄 Session Management

The FP service maintains sophisticated session tracking for both client and server operations.

### Session Types

#### Client Sessions (`fp_disp_client_session_t`)
**Purpose:** Track outgoing requests and match responses

**Lifecycle:**
1. Created when sending request
2. Tracks timeout for response
3. Matches incoming response by `cmd_id`
4. Closed when response received or timeout

**Key Fields:**
```c
typedef struct {
    fp_disp_session_base_t base;           // Common session data
    fp_req_type_t req_type;                // SYNC or ASYNC
    fp_generic_response_handler_t p_user_handler; // Response callback
    uint8_t *p_resp_data;                  // Response buffer (sync only)
    void *p_session_event;                 // Event for sync waiting
} fp_disp_client_session_t;
```

#### Server Sessions (`fp_disp_server_session_t`)
**Purpose:** Track ARM-and-FIRE operations

**Lifecycle:**
1. Created on ARM request
2. Stores request hash and parameters
3. Validates FIRE request data
4. Closed on FIRE, DISARM, or timeout

**Key Fields:**
```c
typedef struct {
    fp_disp_session_base_t base;           // Common session data
    uint16_t req_data_hash;                // Hash of ARM request data
} fp_disp_server_session_t;
```

### Session Configuration

**Limits (configurable in `fp_config.h`):**
```c
#define MAX_CLIENT_SESSIONS_COUNT ((uint16_t)5)
#define MAX_SERVER_SESSIONS_COUNT ((uint16_t)5)
#define ARMED_REQ_TOUT_S ((uint16_t)(15 * 60))  // 15 minutes
```

### Session Metrics

The system tracks session statistics:
```c
typedef struct {
    uint32_t active_session_cnt;     // Currently active sessions
    uint32_t opened_session_cnt;     // Total sessions opened
    uint32_t closed_session_cnt;     // Total sessions closed
    uint32_t timed_out_session_cnt;  // Sessions that timed out
} fp_disp_session_metrics_t;
```

## ⚙️ Architecture Interface Implementation (`fp_arch.h`)

The `config/fp_arch.h.template` file defines the platform-specific interface that must be implemented by the host system. This interface abstracts away platform dependencies and enables the FP service to run on different operating systems and hardware platforms.

### 🔧 Required Interface Functions

#### Event Management
These functions provide thread synchronization for synchronous FP requests:

```c
void fp_arch_init_event(void **pp_event);
bool fp_arch_wait_for_event(void *p_event, const uint32_t timeout_ms);
void fp_arch_signal_event(void *p_event);
```

**Purpose:** Enable blocking/unblocking of threads waiting for FP responses.

**Implementation Example (CMSIS-RTOS2):**
```c
void fp_arch_init_event(void **pp_event) {
    osEventFlagsId_t *p_event_id = (osEventFlagsId_t *)pp_event;

    if (*p_event_id == NULL) {
        *p_event_id = osEventFlagsNew(NULL);
        assert(*p_event_id != NULL);
    }
}

bool fp_arch_wait_for_event(void *p_event, const uint32_t timeout_ms) {
    osEventFlagsId_t event_id = (osEventFlagsId_t)p_event;
    uint32_t flags = osEventFlagsWait(event_id, 0x01,
                                     osFlagsWaitAny | osFlagsNoClear,
                                     timeout_ms);
    return (flags & 0x01) != 0;
}

void fp_arch_signal_event(void *p_event) {
    osEventFlagsId_t event_id = (osEventFlagsId_t)p_event;
    osEventFlagsSet(event_id, 0x01);
    osEventFlagsClear(event_id, 0x01);
}
```

#### Thread Synchronization
These functions provide mutual exclusion for protecting shared data:

* **Registry Lock:** Global lock for FIDL registry access<br>
* **Section Locks:** Context-specific locks for session data

```c
bool fp_arch_lock_registry(void);
void fp_arch_unlock_registry(void);
bool fp_arch_lock_section(void *const p_lock);
void fp_arch_unlock_section(void *const p_lock);
```

**Implementation Example (CMSIS-RTOS2):**
```c
static osMutexId_t g_registry_mutex = NULL;

bool fp_arch_lock_registry(void) {
    if (g_registry_mutex == NULL) {
        g_registry_mutex = osMutexNew(NULL);
        assert(g_registry_mutex != NULL);
    }

    osStatus_t status = osMutexAcquire(g_registry_mutex, osWaitForever);
    return (status == osOK);
}

void fp_arch_unlock_registry(void) {
    if (g_registry_mutex != NULL) {
        osMutexRelease(g_registry_mutex);
    }
}

bool fp_arch_lock_section(void *const p_lock) {
    osMutexId_t mutex = (osMutexId_t)p_lock;
    if (mutex == NULL) return false;

    osStatus_t status = osMutexAcquire(mutex, osWaitForever);
    return (status == osOK);
}

void fp_arch_unlock_section(void *const p_lock) {
    osMutexId_t mutex = (osMutexId_t)p_lock;
    if (mutex != NULL) {
        osMutexRelease(mutex);
    }
}
```

#### Timer Services
These functions provide timeout management for sessions:

```c
void fp_arch_start_timer(fp_arch_timer_handle_t *const p_timer_hnd);
void fp_arch_stop_timer(fp_arch_timer_handle_t *const p_timer_hnd);
bool fp_arch_is_timer_elapsed(fp_arch_timer_handle_t *const p_timer_hnd,
                              const uint32_t elapse_time);
```

**Implementation Example (System Tick):**
```c
typedef uint32_t fp_arch_timer_handle_t;

void fp_arch_start_timer(fp_arch_timer_handle_t *const p_timer_hnd) {
    if (p_timer_hnd != NULL) {
        *p_timer_hnd = osKernelGetTickCount();
    }
}

void fp_arch_stop_timer(fp_arch_timer_handle_t *const p_timer_hnd) {
    if (p_timer_hnd != NULL) {
        *p_timer_hnd = 0;
    }
}

bool fp_arch_is_timer_elapsed(fp_arch_timer_handle_t *const p_timer_hnd,
                              const uint32_t elapse_time) {
    if (p_timer_hnd == NULL || *p_timer_hnd == 0) {
        return true;  // Timer not started or stopped
    }

    uint32_t current_tick = osKernelGetTickCount();
    uint32_t elapsed_ms = (current_tick - *p_timer_hnd) * 1000 / osKernelGetTickFreq();

    return elapsed_ms >= elapse_time;
}
```

#### Hash Calculation
This function provides data integrity validation for ARM-and-FIRE operations. Its purpose is to avoid buffering the request data during the first stage of execution, only to check the data again in the second stage. The hash function can be as simple as a running sum over the data bytes or a more complex hash calculation. Some examples follow...

```c
uint16_t fp_arch_calc_hash(const uint8_t *const p_data, const uint32_t data_sz);
```

**Implementation Example (CRC16):**
```c
uint16_t fp_arch_calc_hash(const uint8_t *const p_data, const uint32_t data_sz) {
    if (p_data == NULL || data_sz == 0) {
        return 0;
    }

    uint16_t crc = 0xFFFF;

    for (uint32_t i = 0; i < data_sz; i++) {
        crc ^= p_data[i];

        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;  // CRC16-ANSI polynomial
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}
```

**Alternative Implementation (Simple Checksum):**
```c
uint16_t fp_arch_calc_hash(const uint8_t *const p_data, const uint32_t data_sz) {
    if (p_data == NULL || data_sz == 0) {
        return 0;
    }

    uint16_t sum = 0;
    for (uint32_t i = 0; i < data_sz; i++) {
        sum += p_data[i];
        sum = (sum << 1) | (sum >> 15);  // Rotate left
    }

    return sum;
}
```

#### Assert Functions
These functions provide debugging and error handling support:

```c
void fp_arch_assert(const bool cond);
void fp_arch_break_assert(const bool cond);
```

**Implementation Example:**
```c
void fp_arch_assert(const bool cond) {
    if (cond) {
        // Log error and halt system
        printf("FP ASSERT FAILED at %s:%d\n", __FILE__, __LINE__);
        while(1) { /* Halt */ }
    }
}

void fp_arch_break_assert(const bool cond) {
    if (cond) {
        // Set debugger breakpoint if available
        #ifdef DEBUG
            __BKPT(0);  // ARM Cortex-M breakpoint
        #else
            fp_arch_assert(cond);
        #endif
    }
}
```

### 🔧 Trace Macros
The template provides trace macros that should be mapped to your logging system:

```c
#define FP_ARCH_TRACE_ERROR(format, ...)   your_log_error(format, ##__VA_ARGS__)
#define FP_ARCH_TRACE_WARN(format, ...)    your_log_warn(format, ##__VA_ARGS__)
#define FP_ARCH_TRACE_INFO(format, ...)    your_log_info(format, ##__VA_ARGS__)
#define FP_ARCH_TRACE_FATAL(format, ...)   your_log_fatal(format, ##__VA_ARGS__)
#define FP_ARCH_TRACE_DEBUG(format, ...)   your_log_debug(format, ##__VA_ARGS__)
```

**Example Implementation:**
```c
#include "your_logging_system.h"

#define FP_ARCH_TRACE_ERROR(format, ...)   LOG_ERROR("[FP] " format, ##__VA_ARGS__)
#define FP_ARCH_TRACE_WARN(format, ...)    LOG_WARN("[FP] " format, ##__VA_ARGS__)
#define FP_ARCH_TRACE_INFO(format, ...)    LOG_INFO("[FP] " format, ##__VA_ARGS__)
#define FP_ARCH_TRACE_FATAL(format, ...)   LOG_FATAL("[FP] " format, ##__VA_ARGS__)
#define FP_ARCH_TRACE_DEBUG(format, ...)   LOG_DEBUG("[FP] " format, ##__VA_ARGS__)
```

### 🔧 Compiler-Specific Definitions
The template handles different compilers for packed structures:

```c
// GCC and compatible compilers
#ifdef __GNUC__
#define PACK_STRUCT_BEGIN _Pragma("pack(push, 1)")
#define PACK_STRUCT_END   _Pragma("pack(pop)")

// Microsoft Visual C++
#elif defined(_MSC_VER)
#define PACK_STRUCT_BEGIN __pragma(pack(push, 1))
#define PACK_STRUCT_END   __pragma(pack(pop))

// TI MSP430 Compiler
#elif defined(__TI_COMPILER_VERSION__)
#define PACK_STRUCT_BEGIN _Pragma("pack(push, 1)")
#define PACK_STRUCT_END   _Pragma("pack(pop)")
#endif
```

### 🔧 Integration Checklist

When implementing the `fp_arch.h` interface:

- [ ] **Event Management**: Implement thread synchronization primitives
- [ ] **Mutex Support**: Provide locking mechanisms for thread safety
- [ ] **Timer Services**: Implement timeout tracking for sessions
- [ ] **Hash Function**: Choose appropriate algorithm for data integrity
- [ ] **Assert Handling**: Define error handling and debugging behavior
- [ ] **Trace Macros**: Map to your logging/debugging system
- [ ] **Compiler Support**: Ensure packed structure definitions work
- [ ] **Testing**: Verify all functions work correctly in your environment

### 🔧 Platform-Specific Considerations

#### RTOS Environments
- Use OS-provided synchronization primitives
- Consider priority inheritance for mutexes
- Implement proper timeout handling
- Account for tick resolution in timers

#### Bare-Metal Systems
- Implement simple spinlocks for synchronization
- Use system tick counter for timing
- Consider interrupt safety
- Minimize memory usage

#### Multi-Core Systems
- Ensure atomic operations where needed
- Consider cache coherency for shared data
- Use appropriate memory barriers
- Test thoroughly for race conditions

## 🚀 Quick Start Guide

### 1. Integration Setup

**Step 1: Copy Configuration Templates**
```bash
cp config/fp_arch.h.template your_project/fp_arch.h
cp config/fp_config.h.template your_project/fp_config.h
cp config/fp_config_user.h.template your_project/fp_config_user.h
cp config/fp_config_user.c.template your_project/fp_config_user.c
```

**Step 2: Implement Platform Interface**
Edit `fp_arch.h` to implement the required functions for your platform:
```c
// Example for CMSIS-RTOS2
#include "cmsis_os2.h"

void fp_arch_init_event(void **pp_event) {
    // Implement event initialization
}

bool fp_arch_wait_for_event(void *p_event, const uint32_t timeout_ms) {
    // Implement event waiting
}

// ... implement other required functions
```

**Step 3: Configure System Parameters**
Edit `fp_config.h` to set system limits:
```c
#define MAX_CLIENT_SESSIONS_COUNT ((uint16_t)10)
#define MAX_SERVER_SESSIONS_COUNT ((uint16_t)5)
#define ARMED_REQ_TOUT_S ((uint16_t)(15 * 60))
```

### 2. Basic Usage Example

All examples below assume that the underlying FP transport layer is properly configured and functional.

**Client Side (Sending Requests):**
```c
#include "fp_disp_client.h"
#include "fp_config_user.h"

// Initialize FP system
fp_config_user_init();

// Setup dispatch context
fp_dispatch_info_t disp_ctx;
fp_config_user_assign_sys_context(&disp_ctx, FP_CONFIG_SYS_CTX_TYPE_CSP);

// Send synchronous request
uint8_t request_buffer[64];
uint8_t response_buffer[64];

fp_cmd_status_t status = fp_disp_client_send_req(
    &disp_ctx,
    0x1234,                    // interface ID
    0x5678,                    // function ID
    request_buffer,            // request data
    sizeof(request_buffer),    // request size
    response_buffer,           // response buffer
    NULL,                      // sync request (no callback)
    5000                       // 5 second timeout
);

if (status == FP_CMD_STATUS_NOERROR) {
    // Process response_buffer
}
```

**Server Side (Handling Requests):**
```c
#include "fp_disp_server.h"
#include "fp_registry.h"

// Request handler function
void my_request_handler(fp_dispatch_info_t *p_disp_ctx, void *p_req_data) {
    // Process request
    uint8_t response_data[32];

    // Prepare response
    // ...

    // Send response
    fp_disp_server_send_resp(p_disp_ctx, 0x1234, 0x5678,
                            response_data, sizeof(response_data));
}

// Register FIDL interface
fidl_if_t my_interface = {
    .iface_id = 0x1234,
    .type = FIDL_TYPE_SERVER,
    .p_req_handler = my_request_handler
};

fp_registry_add_fidl_if(&my_interface);
```

### 3. ARM-and-FIRE Example

**Client Side:**
```c
// Step 1: Send ARM request
fp_cmd_status_t arm_status = fp_disp_client_send_req(
    &disp_ctx, 0x1234, 0x5678, FP_FRAME_TYPE_REQ_ARM,
    request_buffer, sizeof(request_buffer), response_buffer, NULL, 5000);

if (arm_status == FP_CMD_STATUS_ARMED) {
    // Step 2: User confirmation
    printf("Operation armed. Confirm execution? (y/n): ");
    char confirm = getchar();

    if (confirm == 'y') {
        // Step 3: Send FIRE request with same data
        fp_cmd_status_t fire_status = fp_disp_client_send_req(
            &disp_ctx, 0x1234, 0x5678, FP_FRAME_TYPE_REQ_FIRE,
            request_buffer, sizeof(request_buffer), response_buffer, NULL, 5000);

        if (fire_status == FP_CMD_STATUS_NOERROR) {
            printf("Operation executed successfully\n");
        }
    } else {
        // Step 3: Send DISARM request
        fp_disp_client_send_req(&disp_ctx, 0x1234, 0x5678, FP_FRAME_TYPE_REQ_DISARM,
                               request_buffer, sizeof(request_buffer),
                               response_buffer, NULL, 5000);
    }
}
```

**Server Side:**
```c
void critical_operation_handler(fp_dispatch_info_t *p_disp_ctx, void *p_req_data) {
    // This handler automatically supports ARM-and-FIRE
    // The FP service handles the ARM/FIRE logic

    // This code only executes after successful FIRE
    printf("Executing critical operation\n");

    // Perform the actual operation
    // ...

    // Send response
    uint8_t response[16] = "Operation complete";
    fp_disp_server_send_resp(p_disp_ctx, 0x1234, 0x5678,
                            response, sizeof(response));
}

// Register with ARM-and-FIRE support
fidl_if_t critical_interface = {
    .iface_id = 0x1234,
    .type = FIDL_TYPE_SERVER,
    .p_req_handler = critical_operation_handler,
    .req_type = FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE  // Enable ARM-and-FIRE
};
```

## 📊 Performance Considerations

### Memory Usage
- **Session Storage**: Pre-allocated pools prevent dynamic allocation
- **Zero-Copy**: Communication buffers passed by reference
- **Minimal Overhead**: Efficient session tracking and management

### Timing Characteristics
- **Session Lookup**: O(n) where n = max sessions (typically ≤ 10)
- **Hash Calculation**: O(m) where m = message size
- **Lock Contention**: Minimized through context-specific locking

### Scalability
- **Concurrent Sessions**: Limited by configuration (default: 5 client + 5 server)
- **Transport Independence**: Multiple contexts can operate simultaneously
- **Thread Safety**: Full multi-threaded operation support

## 🔍 Troubleshooting

### Common Issues

#### "Session allocation failed" (FP_CMD_STATUS_NO_MEM)
**Cause:** All sessions in use or lock timeout
**Solution:**
- Increase `MAX_CLIENT_SESSIONS_COUNT` in `fp_config.h`
- Check for session leaks (responses not received)
- Verify lock implementation doesn't deadlock

#### "Data integrity error" (FP_CMD_STATUS_DATA_INTEGRITY_ERR)
**Cause:** Request data changed between ARM and FIRE
**Solution:**
- Ensure identical data in ARM and FIRE requests
- Check for memory corruption
- Verify hash function implementation

#### "Function not supported" (FP_CMD_STATUS_FUNC_NOT_SUPPORTED)
**Cause:** FIDL interface not registered or ARM session expired
**Solution:**
- Verify `fp_registry_add_fidl_if()` called
- Check interface ID and function ID match
- For ARM-and-FIRE: ensure FIRE sent within timeout

#### Timeout Issues
**Cause:** Network delays, processing delays, or incorrect timeout values
**Solution:**
- Increase timeout values for slow operations
- Check transport layer connectivity
- Verify timer implementation accuracy

### Debug Techniques

#### Enable Trace Logging
```c
#define FP_ARCH_TRACE_DEBUG(format, ...) printf("[FP_DEBUG] " format "\n", ##__VA_ARGS__)
#define FP_ARCH_TRACE_INFO(format, ...)  printf("[FP_INFO] " format "\n", ##__VA_ARGS__)
#define FP_ARCH_TRACE_ERROR(format, ...) printf("[FP_ERROR] " format "\n", ##__VA_ARGS__)
```

#### Session Monitoring
```c
// Check session metrics
fp_disp_session_metrics_t *client_metrics = &sys_ctx.client_metrics;
printf("Active sessions: %u\n", client_metrics->active_session_cnt);
printf("Timed out sessions: %u\n", client_metrics->timed_out_session_cnt);
```

#### Message Inspection
```c
// Log message headers
fp_msg_hdr_t *hdr = (fp_msg_hdr_t *)message_buffer;
printf("iface_id: 0x%04X, func_id: 0x%08X, cmd_id: 0x%04X, status: 0x%02X\n",
       hdr->iface_id, hdr->func_id, hdr->cmd_id, hdr->status);
```

## 📚 API Reference

### Core Functions

#### Client API
```c
fp_cmd_status_t fp_disp_client_send_req(
    fp_dispatch_info_t *p_disp_ctx,
    const uint16_t iface_id,
    const uint32_t func_id,
    const fp_frame_type_t frame_type,
    uint8_t *const p_req,
    const uint32_t req_size,
    uint8_t *const p_resp,
    fp_generic_response_handler_t p_resp_handler,
    const uint32_t timeout
);

void fp_disp_client_handle_resp(
    fp_dispatch_info_t *p_disp_ctx,
    const uint32_t resp_type_size
);

void fp_disp_client_handle_bcast(
    fp_dispatch_info_t *p_disp_ctx,
    const uint32_t bcast_type_size,
    fp_generic_broadcast_handler_t p_bcast_handler
);
```

#### Server API
```c
void fp_disp_server_handle_req(
    fp_dispatch_info_t *p_disp_ctx,
    const fp_disp_server_req_type_t req_type,
    fp_generic_request_handler_t p_req_handler,
    const uint32_t req_type_size
);

bool fp_disp_server_send_resp(
    fp_dispatch_info_t *p_disp_ctx,
    const uint16_t iface_id,
    const uint32_t func_id,
    uint8_t *const p_resp,
    const uint32_t resp_size
);
```

#### Registry API
```c
fp_cmd_status_t fp_registry_add_fidl_if(const fidl_if_t *const p_fidl_if);
fp_cmd_status_t fp_registry_remove_fidl_if(const fidl_if_t *const p_fidl_if);
const fidl_if_t *fp_registry_get_fidl_if_by_id(const uint16_t iface_id,
                                               const fidl_type_t fidl_type);
```

#### Protocol Utilities
```c
void fp_proto_get_func_attributes(const fp_msg_hdr_t *const p_fp_hdr,
                                 fp_proto_func_attr_t *const p_attr);
void fp_proto_set_frame_type(fp_msg_hdr_t *const p_fp_hdr,
                             const fp_frame_type_t frame_type);
void fp_proto_set_func_id(fp_msg_hdr_t *const p_fp_hdr,
                          const uint32_t func_id);
bool fp_proto_is_bare_func_id_matching(const uint32_t func_id_1,
                                       const uint32_t func_id_2);
```

## 📈 Testing

### Unit Tests
The `test/` directory contains comprehensive unit tests covering:
- Protocol message handling
- Session management
- ARM-and-FIRE logic
- Error conditions
- Platform interface mocking

### Integration Testing
Recommended integration test scenarios:
1. **Basic Request/Response**: Verify normal operation
2. **ARM-and-FIRE Flow**: Test critical operation handling
3. **Timeout Scenarios**: Verify timeout handling and cleanup
4. **Error Conditions**: Test error propagation and recovery
5. **Concurrent Operations**: Multi-threaded stress testing
6. **Transport Integration**: Test with actual transport layers

### Building and Running the Tests

Requirements: CMake 3.14 or newer, a C11 and C++14 compiler (GCC or Clang),
git, and [gcovr](https://gcovr.com) for the coverage target. Catch2 v3 is used
from the system when installed, otherwise it is fetched from GitHub. The
`.devcontainer.json` provides a ready environment.

```bash
cmake -S test -B build/test
cmake --build build/test -j
./build/test/start-test

# Coverage report (HTML and Cobertura) in build/test/cov-report
cmake --build build/test --target gen-test-coverage
```

### Diagrams
```bash
cd diagrams
./generate_svg.sh
# Open svg/*.svg files in a browser
```

## 🤝 Contributing

Contributions are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md) for how to
report issues and propose changes, and [SECURITY.md](SECURITY.md) for reporting
vulnerabilities.

### Development Guidelines
1. **Code Style**: Follow existing C coding conventions
2. **Documentation**: Update diagrams and README for changes
3. **Testing**: Add tests for new functionality
4. **Platform Support**: Maintain compatibility across platforms

### Diagram Updates
When modifying the system:
1. Update relevant PlantUML diagrams
2. Regenerate SVG files: `cd diagrams && ./generate_svg.sh`
3. Verify diagrams reflect actual implementation

## 📄 License

FP is source-available under the EnduroSat Software License v1.0. See
[LICENSE](LICENSE) for the full terms.

Copyright 2026 ENDUROSAT EAD

---

**For technical support or questions about the FP service, please contact [EnduroSat support](https://www.endurosat.com/contact/).**
