routerAdd("GET", "/api/openpocketbase-test/delay", (event) => {
  sleep(2500);
  return event.json(200, {
    message: "Delayed fixture response",
    delayMilliseconds: 2500,
  });
});
