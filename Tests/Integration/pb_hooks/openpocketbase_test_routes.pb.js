routerAdd("GET", "/api/openpocketbase-test/delay", (event) => {
  sleep(2500);
  return event.json(200, {
    message: "Delayed fixture response",
    delayMilliseconds: 2500,
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
  event.response.header().set("Content-Type", "application/octet-stream");
  event.response.writeHeader(200);
  event.response.write(toBytes("x".repeat(4096)));
  event.flush();
});

routerAdd("POST", "/api/openpocketbase-test/retry/reset", (event) => {
  try {
    for (const id of ["retryread000001", "retrywrite00001"]) {
      try {
        $app.delete($app.findRecordById("sdk_tasks", id));
      } catch (_) {
      }
    }
    return event.json(200, {
      reset: true,
    });
  } catch (error) {
    return event.json(500, {
      fixtureError: String(error),
    });
  }
});

routerAdd("GET", "/api/openpocketbase-test/retry/read", (event) => {
  let marker;
  try {
    marker = $app.findRecordById("sdk_tasks", "retryread000001");
    marker.set("score", marker.getInt("score") + 1);
  } catch (_) {
    marker = new Record($app.findCollectionByNameOrId("sdk_tasks"));
    marker.set("id", "retryread000001");
    marker.set("title", "Retry read fixture marker");
    marker.set("done", false);
    marker.set("score", 1);
  }
  $app.save(marker);
  const attempts = marker.getInt("score");
  if (attempts === 1) {
    return event.json(503, {
      code: "fixture_read_retry",
      message: "The first eligible read attempt fails on purpose.",
      attempts: attempts,
    });
  }
  return event.json(200, {
    eligibleReadSucceeded: true,
    attempts: attempts,
  });
});

routerAdd("POST", "/api/openpocketbase-test/retry/write", (event) => {
  let marker;
  try {
    marker = $app.findRecordById("sdk_tasks", "retrywrite00001");
    marker.set("score", marker.getInt("score") + 1);
  } catch (_) {
    marker = new Record($app.findCollectionByNameOrId("sdk_tasks"));
    marker.set("id", "retrywrite00001");
    marker.set("title", "Retry write fixture marker");
    marker.set("done", false);
    marker.set("score", 1);
  }
  $app.save(marker);
  const attempts = marker.getInt("score");
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
