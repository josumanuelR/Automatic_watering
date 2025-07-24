/// <reference path="../pb_data/types.d.ts" />
migrate((app) => {
  const collection = app.findCollectionByNameOrId("pbc_3830102650")

  // update collection data
  unmarshal({
    "name": "water_tank_level"
  }, collection)

  return app.save(collection)
}, (app) => {
  const collection = app.findCollectionByNameOrId("pbc_3830102650")

  // update collection data
  unmarshal({
    "name": "tank_water_level"
  }, collection)

  return app.save(collection)
})
