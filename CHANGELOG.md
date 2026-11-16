# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
