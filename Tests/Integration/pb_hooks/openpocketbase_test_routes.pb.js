// PocketBase serializes route callbacks independently, so request-time helpers stay inside each callback.
routerAdd("GET", "/api/openpocketbase-test/delay", (event) => {
  const parseInteger = (value, fallback, minimum, maximum, name) => {
    if (value === "") {
      return fallback;
    }
    const trimmed = value.trim();
    const parsed = Number(trimmed);
    if (trimmed === "" || !isFinite(parsed) || Math.floor(parsed) !== parsed) {
      throw new BadRequestError(name + " must be an integer.");
    }
    return Math.max(minimum, Math.min(maximum, Math.floor(parsed)));
  };
  const requestedDelayText = event.request.url.query().get("milliseconds");
  const delayMilliseconds = parseInteger(requestedDelayText, 2500, 0, 5000, "milliseconds");
  sleep(delayMilliseconds);
  return event.json(200, {
    message: "Delayed fixture response",
    delayMilliseconds: delayMilliseconds,
  });
});

routerAdd("POST", "/api/openpocketbase-test/delay", (event) => {
  const parseInteger = (value, fallback, minimum, maximum, name) => {
    if (value === "") {
      return fallback;
    }
    const trimmed = value.trim();
    const parsed = Number(trimmed);
    if (trimmed === "" || !isFinite(parsed) || Math.floor(parsed) !== parsed) {
      throw new BadRequestError(name + " must be an integer.");
    }
    return Math.max(minimum, Math.min(maximum, Math.floor(parsed)));
  };
  const requestedDelayText = event.request.url.query().get("milliseconds");
  const delayMilliseconds = parseInteger(requestedDelayText, 1000, 0, 5000, "milliseconds");
  sleep(delayMilliseconds);
  return event.json(200, {
    message: "Delayed fixture write response",
    delayMilliseconds: delayMilliseconds,
    label: event.request.url.query().get("label"),
  });
});

routerAdd("GET", "/api/openpocketbase-test/stall", (event) => {
  event.response.header().set("Content-Type", "application/json");
  event.response.writeHeader(200);
  event.response.write(toBytes('{"phase":"started",'));
  event.flush();
  sleep(2500);
  event.response.write(toBytes('"done":true}'));
  event.flush();
});

routerAdd("GET", "/api/openpocketbase-test/large-response", (event) => {
  const parseInteger = (value, fallback, minimum, maximum, name) => {
    if (value === "") {
      return fallback;
    }
    const trimmed = value.trim();
    const parsed = Number(trimmed);
    if (trimmed === "" || !isFinite(parsed) || Math.floor(parsed) !== parsed) {
      throw new BadRequestError(name + " must be an integer.");
    }
    return Math.max(minimum, Math.min(maximum, Math.floor(parsed)));
  };
  const bytesQuery = event.request.url.query().get("bytes");
  const requestedBytesText = bytesQuery !== ""
    ? bytesQuery
    : event.request.url.query().get("size");
  const responseBytes = parseInteger(requestedBytesText, 4096, 0, 1048576, "bytes");
  event.response.header().set("Content-Type", "application/octet-stream");
  event.response.writeHeader(200);
  event.response.write(toBytes("x".repeat(responseBytes)));
  event.flush();
});

routerAdd("GET", "/api/openpocketbase-test/periodic", (event) => {
  const parseInteger = (value, fallback, minimum, maximum, name) => {
    if (value === "") {
      return fallback;
    }
    const trimmed = value.trim();
    const parsed = Number(trimmed);
    if (trimmed === "" || !isFinite(parsed) || Math.floor(parsed) !== parsed) {
      throw new BadRequestError(name + " must be an integer.");
    }
    return Math.max(minimum, Math.min(maximum, Math.floor(parsed)));
  };
  const requestedChunksText = event.request.url.query().get("chunks");
  const requestedIntervalText = event.request.url.query().get("intervalMilliseconds");
  const chunks = parseInteger(requestedChunksText, 4, 1, 10, "chunks");
  const intervalMilliseconds = parseInteger(
    requestedIntervalText,
    250,
    0,
    2000,
    "intervalMilliseconds"
  );

  event.response.header().set("Content-Type", "application/octet-stream");
  event.response.writeHeader(200);
  for (let index = 0; index < chunks; index += 1) {
    event.response.write(toBytes("chunk-" + index + "\n"));
    event.flush();
    if (index + 1 < chunks) {
      sleep(intervalMilliseconds);
    }
  }
});

routerAdd("GET", "/api/openpocketbase-test/headers", (event) => {
  return event.json(200, {
    acceptLanguage: event.request.header.get("Accept-Language"),
    testHeader: event.request.header.get("X-OpenPocketBase-Test"),
  });
});

routerAdd("POST", "/api/openpocketbase-test/auth/reset", (event) => {
  const remoteIp = event.remoteIP();
  if (
    remoteIp !== "127.0.0.1" &&
    remoteIp !== "::1" &&
    remoteIp !== "0:0:0:0:0:0:0:1" &&
    remoteIp !== "0000:0000:0000:0000:0000:0000:0000:0001"
  ) {
    throw new ForbiddenError("The test fixture route is only available on the local machine.");
  }
  const origin = event.request.header.get("Origin");
  if (origin !== "" && origin !== "http://127.0.0.1:18091") {
    throw new ForbiddenError("The test fixture route is only available to the local test project.");
  }

  try {
    const providers = [
      ["extauth00000001", "github", "fixture-github-user"],
      ["extauth00000002", "google", "fixture-google-user"],
    ];

    $app.runInTransaction((txApp) => {
      const users = txApp.findCollectionByNameOrId("sdk_users");
      let conflictingPlayer = null;
      try {
        conflictingPlayer = txApp.findAuthRecordByEmail("sdk_users", "player@example.com");
      } catch (_) {
      }
      if (conflictingPlayer && conflictingPlayer.id !== "user00000000001") {
        txApp.delete(conflictingPlayer);
      }

      let player;
      try {
        player = txApp.findRecordById(users, "user00000000001");
      } catch (_) {
        player = new Record(users);
        player.set("id", "user00000000001");
      }

      player.set("email", "player@example.com");
      player.set("emailVisibility", true);
      player.set("verified", true);
      player.set("password", "correct-horse-battery");
      player.refreshTokenKey();
      txApp.save(player);
      txApp.deleteAllExternalAuthsByRecord(player);
      txApp.deleteAllMFAsByRecord(player);
      txApp.deleteAllOTPsByRecord(player);

      const temporaryEmails = [
        "chunk28-user@openpocketbase.test",
        "delivered+verify-openpocketbase@resend.dev",
        "delivered+email-change-old@resend.dev",
        "delivered+email-change-new@resend.dev",
      ];
      for (let emailIndex = 0; emailIndex < temporaryEmails.length; emailIndex += 1) {
        const email = temporaryEmails[emailIndex];
        let temporaryUser = null;
        try {
          temporaryUser = txApp.findAuthRecordByEmail("sdk_users", email);
        } catch (_) {
        }
        if (temporaryUser) {
          txApp.delete(temporaryUser);
        }
      }

      const interruptedRegistrations = txApp.findRecordsByFilter(
        users,
        "email ~ {:prefix} && email ~ {:domain}",
        "",
        0,
        0,
        {
          prefix: "chunk28+",
          domain: "@openpocketbase.test",
        }
      );
      for (let recordIndex = 0; recordIndex < interruptedRegistrations.length; recordIndex += 1) {
        txApp.delete(interruptedRegistrations[recordIndex]);
      }

      let conflictingOtpFixture = null;
      try {
        conflictingOtpFixture = txApp.findAuthRecordByEmail(
          "sdk_users",
          "delivered+openpocketbase@resend.dev"
        );
      } catch (_) {
      }
      if (conflictingOtpFixture && conflictingOtpFixture.id !== "otpfixture00001") {
        txApp.delete(conflictingOtpFixture);
      }

      let otpFixture;
      try {
        otpFixture = txApp.findRecordById(users, "otpfixture00001");
      } catch (_) {
        otpFixture = new Record(users);
        otpFixture.set("id", "otpfixture00001");
      }
      otpFixture.set("email", "delivered+openpocketbase@resend.dev");
      otpFixture.set("emailVisibility", true);
      otpFixture.set("verified", true);
      otpFixture.set("password", "otp-fixture-password");
      otpFixture.refreshTokenKey();
      txApp.save(otpFixture);
      txApp.deleteAllExternalAuthsByRecord(otpFixture);
      txApp.deleteAllMFAsByRecord(otpFixture);
      txApp.deleteAllOTPsByRecord(otpFixture);

      let conflictingMfaFixture = null;
      try {
        conflictingMfaFixture = txApp.findAuthRecordByEmail(
          "sdk_users",
          "delivered+mfa-openpocketbase@resend.dev"
        );
      } catch (_) {
      }
      if (conflictingMfaFixture && conflictingMfaFixture.id !== "mfafixture00001") {
        txApp.delete(conflictingMfaFixture);
      }

      let mfaFixture;
      try {
        mfaFixture = txApp.findRecordById(users, "mfafixture00001");
      } catch (_) {
        mfaFixture = new Record(users);
        mfaFixture.set("id", "mfafixture00001");
      }
      mfaFixture.set("email", "delivered+mfa-openpocketbase@resend.dev");
      mfaFixture.set("emailVisibility", true);
      mfaFixture.set("verified", true);
      mfaFixture.set("password", "mfa-fixture-password");
      mfaFixture.refreshTokenKey();
      txApp.save(mfaFixture);
      txApp.deleteAllExternalAuthsByRecord(mfaFixture);
      txApp.deleteAllMFAsByRecord(mfaFixture);
      txApp.deleteAllOTPsByRecord(mfaFixture);

      const externalAuths = txApp.findCollectionByNameOrId("_externalAuths");
      for (let providerIndex = 0; providerIndex < providers.length; providerIndex += 1) {
        const providerConfig = providers[providerIndex];
        const externalAuth = new Record(externalAuths);
        externalAuth.set("id", providerConfig[0]);
        externalAuth.set("collectionRef", users.id);
        externalAuth.set("recordRef", player.id);
        externalAuth.set("provider", providerConfig[1]);
        externalAuth.set("providerId", providerConfig[2]);
        txApp.save(externalAuth);
      }

      const savedExternalAuths = txApp.findAllExternalAuthsByRecord(player);
      if (savedExternalAuths.length !== providers.length) {
        throw new Error("External-auth reset postcondition failed.");
      }
    });

    return event.json(200, {
      reset: true,
      externalAuthCount: providers.length,
    });
  } catch (error) {
    $app.logger().error("OpenPocketBase auth fixture reset failed", "error", error);
    throw new InternalServerError("The OpenPocketBase auth fixture could not be reset.");
  }
});

routerAdd("POST", "/api/openpocketbase-test/retry/reset", (event) => {
  const remoteIp = event.remoteIP();
  if (
    remoteIp !== "127.0.0.1" &&
    remoteIp !== "::1" &&
    remoteIp !== "0:0:0:0:0:0:0:1" &&
    remoteIp !== "0000:0000:0000:0000:0000:0000:0000:0001"
  ) {
    throw new ForbiddenError("The test fixture route is only available on the local machine.");
  }
  const origin = event.request.header.get("Origin");
  if (origin !== "" && origin !== "http://127.0.0.1:18091") {
    throw new ForbiddenError("The test fixture route is only available to the local test project.");
  }
  try {
    $app.runInTransaction((txApp) => {
      const markerIds = ["retryread000001", "retrywrite00001"];
      for (let markerIndex = 0; markerIndex < markerIds.length; markerIndex += 1) {
        let marker = null;
        try {
          marker = txApp.findRecordById("sdk_tasks", markerIds[markerIndex]);
        } catch (_) {
        }
        if (marker) {
          txApp.delete(marker);
        }
      }
    });
    return event.json(200, {
      reset: true,
    });
  } catch (error) {
    $app.logger().error("OpenPocketBase retry fixture reset failed", "error", error);
    throw new InternalServerError("The OpenPocketBase retry fixture could not be reset.");
  }
});

routerAdd("GET", "/api/openpocketbase-test/retry/read", (event) => {
  const remoteIp = event.remoteIP();
  if (
    remoteIp !== "127.0.0.1" &&
    remoteIp !== "::1" &&
    remoteIp !== "0:0:0:0:0:0:0:1" &&
    remoteIp !== "0000:0000:0000:0000:0000:0000:0000:0001"
  ) {
    throw new ForbiddenError("The test fixture route is only available on the local machine.");
  }
  const origin = event.request.header.get("Origin");
  if (origin !== "" && origin !== "http://127.0.0.1:18091") {
    throw new ForbiddenError("The test fixture route is only available to the local test project.");
  }
  const parseInteger = (value, fallback, minimum, maximum, name) => {
    if (value === "") {
      return fallback;
    }
    const trimmed = value.trim();
    const parsed = Number(trimmed);
    if (trimmed === "" || !isFinite(parsed) || Math.floor(parsed) !== parsed) {
      throw new BadRequestError(name + " must be an integer.");
    }
    return Math.max(minimum, Math.min(maximum, Math.floor(parsed)));
  };
  const requestedFailuresText = event.request.url.query().get("failures");
  const failureCount = parseInteger(requestedFailuresText, 1, 0, 10, "failures");
  const requestedStatusText = event.request.url.query().get("status");
  const retryStatuses = [400, 429, 500, 502, 503, 504];
  let failureStatus = 503;
  if (requestedStatusText !== "") {
    const trimmedStatus = requestedStatusText.trim();
    const requestedStatus = Number(trimmedStatus);
    if (
      trimmedStatus === "" ||
      !isFinite(requestedStatus) ||
      Math.floor(requestedStatus) !== requestedStatus ||
      retryStatuses.indexOf(requestedStatus) === -1
    ) {
      throw new BadRequestError("status must be one of 400, 429, 500, 502, 503, or 504.");
    }
    failureStatus = requestedStatus;
  }
  let attempts = 0;
  $app.runInTransaction((txApp) => {
    let marker;
    try {
      marker = txApp.findRecordById("sdk_tasks", "retryread000001");
      marker.set("score", marker.getInt("score") + 1);
    } catch (_) {
      marker = new Record(txApp.findCollectionByNameOrId("sdk_tasks"));
      marker.set("id", "retryread000001");
      marker.set("title", "Retry read fixture marker");
      marker.set("done", false);
      marker.set("score", 1);
    }
    txApp.save(marker);
    attempts = marker.getInt("score");
  });
  if (attempts <= failureCount) {
    if (failureStatus === 429) {
      event.response.header().set("Retry-After", "1");
    }
    return event.json(failureStatus, {
      code: "fixture_read_retry",
      message: failureCount === 1
        ? "The first eligible read attempt fails on purpose."
        : "The eligible read attempt fails on purpose.",
      attempts: attempts,
      failures: failureCount,
    });
  }
  return event.json(200, {
    eligibleReadSucceeded: true,
    attempts: attempts,
  });
});

routerAdd("POST", "/api/openpocketbase-test/retry/write", (event) => {
  const remoteIp = event.remoteIP();
  if (
    remoteIp !== "127.0.0.1" &&
    remoteIp !== "::1" &&
    remoteIp !== "0:0:0:0:0:0:0:1" &&
    remoteIp !== "0000:0000:0000:0000:0000:0000:0000:0001"
  ) {
    throw new ForbiddenError("The test fixture route is only available on the local machine.");
  }
  const origin = event.request.header.get("Origin");
  if (origin !== "" && origin !== "http://127.0.0.1:18091") {
    throw new ForbiddenError("The test fixture route is only available to the local test project.");
  }
  let attempts = 0;
  $app.runInTransaction((txApp) => {
    let marker;
    try {
      marker = txApp.findRecordById("sdk_tasks", "retrywrite00001");
      marker.set("score", marker.getInt("score") + 1);
    } catch (_) {
      marker = new Record(txApp.findCollectionByNameOrId("sdk_tasks"));
      marker.set("id", "retrywrite00001");
      marker.set("title", "Retry write fixture marker");
      marker.set("done", false);
      marker.set("score", 1);
    }
    txApp.save(marker);
    attempts = marker.getInt("score");
  });
  if (attempts === 1) {
    return event.json(503, {
      code: "fixture_write_retry",
      message: "The unsafe write fails on purpose and must not be retried.",
      attempts: attempts,
    });
  }
  return event.json(200, {
    unsafeWriteRetried: true,
    attempts: attempts,
  });
});

routerAdd("GET", "/api/openpocketbase-test/retry/status", (event) => {
  let readAttempts = 0;
  let writeAttempts = 0;
  try {
    readAttempts = $app.findRecordById("sdk_tasks", "retryread000001").getInt("score");
  } catch (_) {
  }
  try {
    writeAttempts = $app.findRecordById("sdk_tasks", "retrywrite00001").getInt("score");
  } catch (_) {
  }
  return event.json(200, {
    readAttempts: readAttempts,
    writeAttempts: writeAttempts,
  });
});
