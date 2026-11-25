# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- A regular server function (one not configured for ARM-and-FIRE) no longer
  runs its handler for an ARM, FIRE or DISARM request frame.
  `fp_disp_server_handle_req` now answers such a frame with status
  `FP_CMD_STATUS_REQ_ATTR_ERR` (0x53) and does not call the handler, as it
  already did for attribute bits that do not match an ARM-and-FIRE function.
  **Behaviour change:** a ground tool or client that sends attribute bits to a
  regular function used to get that function's normal response and now gets
  0x53. Send plain requests to regular functions.
- An asynchronous request no longer gets a timeout after its response. With a
  transport that hands responses to `fp_disp_execute` and does not close the session
  from its `p_track` callback, the response handler ran a second time with
  `FP_CMD_STATUS_TIMEOUT` at the request timeout, and the session, with its request
  context and transport connection, stayed in use until then. The service now stops
  the request timer after the response handler has returned and the next
  `fp_disp_session_mgr_track_sessions` pass closes the session: `p_close`, then the
  release handler, both without any fp lock held. For such transports `p_close` now
  runs right after the response instead of at the timeout. Transports that close the
  session from `p_track` or `p_deliver`, such as fp_csp_binder, are not affected.
- The unit tests no longer abort when they are built with standard-library
  assertions on (for example `-D_GLIBCXX_ASSERTIONS`). The SYNC session
  tracking test read the last element of an empty stub call list; it now
  checks that the list is empty. Contributed by
  [@amirthn6533](https://github.com/amirthn6533).

- A client now gets the status the server sent in its response.
  - `fp_disp_execute` used to set the status of every frame to
    `FP_CMD_STATUS_NOERROR` before the registered callback ran, so an error reply
    with a payload reached the client as a success and the payload was used as a
    valid result. Only request and broadcast frames are reset now.
  - A server sends every status other than `FP_CMD_STATUS_NOERROR` as the header
    alone. Such a reply was reported as `FP_CMD_STATUS_UNEXP_FRM_SIZE`; the client
    now gets the server's status, for example `FP_CMD_STATUS_FUNC_NOT_SUPPORTED`,
    `FP_CMD_STATUS_ARMED` or `FP_CMD_STATUS_DISARMED`.
    `FP_CMD_STATUS_UNEXP_FRM_SIZE` is now reported only for a
    `FP_CMD_STATUS_NOERROR` reply whose length does not match the FIDL response.
  - Response data is handed over only with `FP_CMD_STATUS_NOERROR`. For any other
    status an asynchronous response handler gets `p_data == NULL`, whatever the
    frame size, and a synchronous request returns the status with a zeroed
    response buffer.

## [1.5.1] - 2026-11-20

### Fixed

- A stray response frame whose `cmd_id` matches no request no longer counts as
  dispatched: for a response frame, `fp_disp_execute` now returns `true` only when
  a client session took it. A transport that closes an asynchronous request when
  `fp_disp_execute` returns `true` (such as fp_csp_binder) used to close the
  request on such a frame, drop the real reply and never call the response
  handler. The request now stays open and the real reply is delivered. As before,
  a registered client callback must hand the frame to `fp_disp_client_handle_resp`,
  as generated client code does.
- A frame whose registered callback ran is no longer passed on to the
  unmatched-frame route, so a response is never looked up a second time by
  `cmd_id`.

## [1.5.0] - 2026-11-16

### Added

- Asynchronous client requests take an optional release handler
  (`fp_dispatch_info_t::p_on_released`). The service calls it once, when the
  request's session is closed, after the transport close callback; from then on
  the request's communication parameters may be reused. Applications that pooled
  request contexts and freed them in the response handler could leak transport
  connections; they can now free them in the release handler. The release handler
  runs without any FP lock held.
- Transports can hand a received asynchronous response to the service with the new
  track action `FP_DISP_TRACK_ACTION_DELIVER` and the optional session callback
  `p_deliver`, which runs without the client-session lock.

### Fixed

- The session tracker no longer calls the timeout response handler of an
  asynchronous request under the client-session lock. The handler runs after the
  lock is released, so it can take application locks and call FP functions without
  deadlocking against another thread that sends.
- A session close that cannot take the client-session lock during session tracking
  is retried on the next tracking pass instead of leaving the session in use.

## [1.4.0] - 2026-10-20

### Changed

- FP is published as source-available under the EnduroSat Software License v1.0.
- The unit tests fetch Catch2 from its public GitHub repository when it is not
  installed on the system.
