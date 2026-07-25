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
