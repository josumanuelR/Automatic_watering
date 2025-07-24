import sys
sys.path.insert(1, './database')

from enum import Enum
from fastapi import FastAPI, Body, HTTPException, Response #, JSONResponse
from fastapi_pagination import Page, add_pagination, paginate
from fastapi.middleware.cors import CORSMiddleware
from typing import List
from pydantic import BaseModel
from pocketbase import PocketBase  # Client also works the same
from pocketbase.client import FileUpload
import asyncio
import time




#inicia FastAPI, PocketBase API y CORSMiddleware
# set up CORS to allow requests from the frontend
server = FastAPI()
add_pagination(server) # Add pagination to the server
database = PocketBase('http://localhost:8090') # Initialize PocketBase client

server.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)




#Objecto de paginación
class logs(BaseModel):
    id: str
    log_id: int
    description: str
    created: str
    updated: str


#Declaración de variables
valveNum = 8
soilMdata = []
valvedata = []
namedata = []
presetsdata = []


#Diccionario para DHT11
climaticdata = [
    {
        "id" : 1,
        "temp" : 0
    },
    {
        "id" : 2,
        "humid" : 0.0
    },
    {
        "id" : 3,
        "lux" : 0.0
    },
]

#Diccionario database autosave
pocketbasedata = [
    {
        "state" : True
    }
]    

#Diccionario para app (soil moissture sensor)
for i in range(valveNum):
    soilMdata.append({"id": i+1, "low_moist_limit": 0.2, "high_moist_limit": 0.8, "data": 0.5, "error_code": 1})

#Diccionario para app (valves state)
for i in range(valveNum):
    valvedata.append({"id": i+1, "timer": -1, "state": False})

#Diccionario para app (control)
controldata = [
    {
        "id" : 1,
        "manualOperation" : False
    },
    {
        "id" : 2,
        "manualWatering" : False
    },
    {
        "id" : 3,
        "etapa" : 2
    },
    {
        "id" : 4,
        "try_connect_sensor" : -1
    }
]


#Diccionario para nombre de sensor
for i in range(valveNum):
    namedata.append({"id": i+1, "name": f"Sensor {i+1}"})


#Diccionario para watercontrol (control de agua)
waterdata = [
    {
        "id" : 1,
        "water_flow" : 0.0
    },
    {
        "id" : 2,
        "water_tank_level" : 0.0
    },
    {
        "id" : 3,
        "irrigation" : False
    },
    {
        "id" : 4,
        "tap_water_valve" : False
    },
    {
        "id" : 5,
        "pump" : False
    },
    {
        "id" : 6,
        "drop_water_tank_valve" : False
    },
    {
        "id" : 7,
        "error_code" : -1
    }
]



#función para checker si id existe en diccionario
def checkIdExists(array, id):
    for obj in array:
        if obj["id"] == id:
            return True
    return False



#Request solicitados                                                                                        #Path0
@server.get('/', status_code=418) #check si el servidor esta funcionando
async def watering_can():
    return "I'm a watering can"


#Request del DHT11                                                                                          #Path1
@server.get("/climatic_variables/body", tags = ["Variables climáticas"]) #hace request al cuerpo completo
async def temp_humid_lux():
    return climaticdata

@server.get("/climatic_variables/{id}", tags = ["Variables climáticas"])
async def temp_humid_lux_get(id : int):
    for i in climaticdata:
        if i["id"] == id:
            return i
    raise HTTPException(status_code=404, detail="Item not found")

@server.put("/climatic_variables", tags = ["Variables climáticas"])
async def temp_humid_lux_put(
    temp: int | None = Body(default=None),
    humid: float | None = Body(default=None),
    lux: float | None = Body(default=None)
):


    if temp != None:
        climaticdata[0]["temp"] = temp

    if humid != None:
        climaticdata[1]["humid"] = humid

    if lux != None:
        climaticdata[2]["lux"] = lux
    
    return climaticdata



#Request de database                                                                                        #Path2
@server.get("/database_state", tags = ["Estado de pocketbase"]) #hace request al estado de la base de datos
async def database_state_get():
    return pocketbasedata

@server.put("/database_state", tags = ["Estado de pocketbase"])
async def database_state_put(state : bool):
    pocketbasedata[0]["state"] = state
    return pocketbasedata

    


#Request de app (Soil moisture sensor)                                                                      #Path3
@server.get("/soil_moisture_data/body", tags = ["Humedad del suelo"]) #hace request al cuerpo completo
async def soil_moisture():
    return soilMdata

@server.get("/soil_moisture_data/{id}", tags = ["Humedad del suelo"])
async def soil_moisture_get(id : int):
    for i in soilMdata:
        if i["id"] == id:
            return i

    return Response(status_code=404)

@server.put("/soil_moisture_data/{id}", tags = ["Humedad del suelo"])
async def soil_moisture_put(
    id: int,
    low_moist_limit: float | None  = Body(default=None),
    high_moist_limit: float | None  = Body(default=None),
    data: float | None  = Body(default=None),
    error_code: int | None  = Body(default=None)
    ):

    if not checkIdExists(soilMdata, id):
        raise HTTPException(status_code=404, detail="Item not found")
    
    send_data = False

    for i in soilMdata:
        if i["id"] == id:
            if i["low_moist_limit"] != low_moist_limit or i["high_moist_limit"] != high_moist_limit:
                send_data = True
            i["low_moist_limit"] = low_moist_limit
            i["high_moist_limit"] = high_moist_limit
            i["data"] = data
            i["error_code"] = error_code
            if send_data:
                await update_data_database(id-1)
                send_data = False
            if i["error_code"] > 0:
                await send_log_database(3, f"Sensor de humedad del suelo {id} en estado de error: error {i['error_code']}")
            return i
    return[]
    


#Request de app (Valves)                                                                                    #Path4
@server.get("/valves_state/body", tags = ["Estado de válvulas"]) #hace request al cuerpo completo
async def valves():
    return valvedata

@server.get("/valves_state/{id}", tags = ["Estado de válvulas"])
async def valves_get(id : int):
    for i in valvedata:
        if i["id"] == id:
            return i
    raise HTTPException(status_code=404, detail="Item not found")

@server.put("/valves_state/{id}", tags = ["Estado de válvulas"])
async def valves_put(
    id: int,
    timer: int | None = Body(default=None),
    state: bool | None = Body(default=None)
):
    if not checkIdExists(valvedata, id):
        raise HTTPException(status_code=404, detail="Item not found")

    for i in valvedata:
        if i["id"] == id:
            i["timer"] = timer
            i["state"] = state
            await sensor_data_create(id-1)
            if i["timer"] > 0:
                await send_log_database(2, f"Válvula {id} activada por {timer-time.time()} segundos")
            await send_log_database(2, f"Estado de válvula {id}: {state}")
            return i
    return[]
    


#Request de app (control)                                                                                   #Path5
@server.get("/control/body", tags = ["Sistema de control"]) #hace request al cuerpo completo
async def control():
    return controldata

@server.get("/control/{id}", tags = ["Sistema de control"])
async def control_get(id : int):
    for i in controldata:
        if i["id"] == id:
            return i
    raise HTTPException(status_code=404, detail="Item not found")

@server.put("/control/{id}", tags = ["Sistema de control"])
async def control_put(
    id: int,
    manualOperation: bool | None = Body(default=None),
    manualWatering: bool | None = Body(default=None),
    etapa: int | None = Body(default=None),
    try_connect_sensor: int | None = Body(default=None)
):

    match id:
        case 1:
            if manualOperation == None:
                return Response(status_code=204)
            controldata[0]["manualOperation"] = manualOperation
            return controldata[0]
        
        case 2:
            if manualWatering == None:
                return Response(status_code=204)
            controldata[1]["manualWatering"] = manualWatering
            return controldata[1]

        case 3:
            if etapa == None:
                return Response(status_code=204)
            controldata[2]["etapa"] = etapa
            match etapa:
                case 0:
                    await send_log_database(1, "Modo manual, válvulas cerradas: etapa 0")
                case 1:
                    await send_log_database(1, "Modo manual, válvulas abiertas: etapa 1")
                case 2:
                    await send_log_database(1, "Modo automático: etapa 2")
            return controldata[2]
    
        case 4:
            if try_connect_sensor == None:
                return Response(status_code=204)
            controldata[3]["try_connect_sensor"] = try_connect_sensor
            return controldata[3]  

        case _:
            if not checkIdExists(controldata, id):
                raise HTTPException(status_code=404, detail="Item not found")
            return []


#Request nombre de sensor                                                                                 #Path6
@server.get("/name_sensor/body", tags = ["Infomación de sensores"]) #hace request al cuerpo completo
async def name_sensor():
    return namedata

@server.get("/name_sensor/{id}", tags = ["Infomación de sensores"])
async def name_sensor_get(id : int):
    for i in namedata:
        if i["id"] == id:
            return i
    raise HTTPException(status_code=404, detail="Item not found")

@server.put("/name_sensor/{id}", tags = ["Infomación de sensores"])
async def name_sensor_put(
    id: int,
    name: str | None = Body(default=None)
):
    if not checkIdExists(namedata, id):
        raise HTTPException(status_code=404, detail="Item not found")
    
    for i in namedata:
        if i["id"] == id:
            i["name"] = name
            await update_data_database(id-1)
            return i
    return[]


#Request de waterapp (water_data)                                                                                   #Path7
@server.get("/water_data/body", tags = ["gestión del agua"]) #hace request al cuerpo completo
async def water_data():
    return waterdata

@server.get("/water_data/{id}", tags = ["gestión del agua"])
async def water_data_get(id : int):
    for i in waterdata:
        if i["id"] == id:
            return i
    raise HTTPException(status_code=404, detail="Item not found")

@server.put("/water_data/{id}", tags = ["gestión del agua"])
async def water_data_put(
    id: int,
    water_flow: float | None = Body(default=None),
    water_tank_level: float | None = Body(default=None),
    irrigation: bool | None = Body(default=None),
    tap_water_valve: bool | None = Body(default=None),
    pump: bool | None = Body(default=None),
    drop_water_tank_valve: bool | None = Body(default=None),
    error_code: int | None = Body(default=None)
):

    match id:
        case 1:
            if water_flow == None:
                return Response(status_code=204)
            waterdata[0]["water_flow"] = water_flow
            return waterdata[0]
        
        case 2:
            if water_tank_level == None:
                return Response(status_code=204)
            waterdata[1]["water_tank_level"] = water_tank_level
            return waterdata[1]

        case 3: #Es una variable que genera el servidor si cualquier valvedata[i] == true
            return Response("This endpoint is read-only", status_code=405)
    
        case 4:
            if tap_water_valve == None:
                return Response(status_code=204)
            waterdata[3]["tap_water_valve"] = tap_water_valve
            return waterdata[3]
        
        case 5:
            if pump == None:
                return Response(status_code=204)
            waterdata[4]["pump"] = pump
            return waterdata[4]

        case 6:
            if drop_water_tank_valve == None:
                return Response(status_code=204)
            waterdata[5]["drop_water_tank_valve"] = drop_water_tank_valve
            return waterdata[5]
        
        case 7:
            if error_code == None:
                return Response(status_code=204)
            waterdata[6]["error_code"] = error_code
            return waterdata[6]

        case _:
            if not checkIdExists(waterdata, id):
                raise HTTPException(status_code=404, detail="Item not found")
            return []


async def check_irrigation(): # Check if irrigation is active
    while True:

        await asyncio.sleep(1)
        
        # If no valves are open, set irrigation to False
        data = False
        
        for i in range(valveNum): # Check if any valve is open
            if valvedata[i]["state"]:
                # If any valve is open, set irrigation to True
                data = True
        
        waterdata[2]["irrigation"] = data


####                                                                               Querys a la base de datos
@server.get("/logs_database/{page}", tags = ["colección logs"])
async def logs_database_query(page: int):
    result = database.collection("logs").get_list(page)

    return {
        "items": result.items,
        "totalItems": result.total_items,
        "totalPages": result.total_pages,
        "page": result.page,
        "perPage": result.per_page,
    }



#sensoresHumedad = "1,3,8"
@server.get("/graphic_data", tags = ["Colección de datos gráficos"])
async def graphic_data_query(humidity: bool = False, temperature: bool = False, sensoresHumedad: str = ""):
    if len(sensoresHumedad) > 0:
        sensoresHumedad_list = []
        for i in sensoresHumedad.split(","):
            sensoresHumedad_list.append(int(i))
    data = {}
    if humidity:
        data["humidity"] = (database.collection("relative_humidity").get_list(1)).items

    if temperature:
        data["temperature"] = (database.collection("temperature").get_list(1)).items

    if len(sensoresHumedad_list) > 0:
        # Build filter string like "id=1 || id=2 || id=3"
        data["sensoresHumedad"] = {}
        for id in sensoresHumedad_list:
            data["sensoresHumedad"][id] = (database.collection(f"{id:02}_soilM_and_valve_data")).get_list(1).items
    return data




###############################################################################################################################
#Requests a la base de datos                                                        DATABASE
async def database_connect():
    try:
        # Authenticate as admin
        admin_data = database.admins.auth_with_password("josue30_97@hotmail.com", "wabibabo")
        print(f"Database connected as following user: {admin_data.is_valid}")
        return admin_data.is_valid
    except Exception as e:
        print(f"Database connection error: {e}") # Catch error
        return False



@server.on_event("startup")
async def startup_event():
    # Try to connect to database on startup please don't fail
    connected = await database_connect()
    if not connected:
        print("WARNING: Could not connect to database at startup")
    
    # Start the background task for database syncing
    asyncio.create_task(send_sensors_data_database())
    asyncio.create_task(download_data_database())
    asyncio.create_task(check_irrigation())
    print("Background tasks started")



async def database_create(collection, body): #Crear un registro en la base de datos
    try:
        result = database.collection(collection).create(body)
        print(f"Data saved to {collection}: {body}")
        return result
    except Exception as e:
        print(f"Database creation error in {collection}: {e}")
        return None



async def database_getFullList(collection = str): #Obtener todos los registros de una colección
    try:
        result = database.collection(collection).get_full_list()
        print(f"Getting data from {collection}")
        return result
    except Exception as e:
        print(f"Database creation error in {collection}: {e}")
        return None



async def database_update(collection, record_id, body): #Actualizar un registro en la base de datos
    try:
        result = database.collection(collection).update(record_id, body)
        print(f"Data saved to {collection}: {body}")
        return result
    except Exception as e:
        print(f"Database creation error in {collection}: {e}")
        return None



async def send_sensors_data_database(): # Enviar temp, hum, y sensor data a la base de datos cada 20 minutos
    print("Starting database sync task...")
    while True:

        await asyncio.sleep(1200)
        # Wait 1200 seconds before next sync? Should be adjustable just in case pa que sepa

        if pocketbasedata[0]["state"]:

            try:
                # Format data for database insert
                temp_data = {
                    "temp": climaticdata[0]["temp"]
                }
                
                # Send temperature data to database
                result = await database_create("temperature", temp_data) #Envia los datos de temperatura a la base de datos
                if result is None:
                    print("Failed to save temperature data")
                    
                # Format data for database insert
                humid_data = {
                    "humd": climaticdata[1]["humid"]
                }

                # Send Relative humidity data to database
                result = await database_create("relative_humidity", humid_data) #Envia los datos de humedad relativa a la base de datos
                if result is None:
                    print("Failed to save relative humidity data")
                
                # Format data for database insert
                lux_data = {
                    "lux": climaticdata[2]["lux"]
                }

                # Send Light data to database
                result = await database_create("light", lux_data) #Envia los datos de luz a la base de datos
                if result is None:
                    print("Failed to save light data")

                
                # Format data for database insert
                water_tank_level_data = {
                    "level": waterdata[1]["water_tank_level"]
                }

                # Send Water tank level data to database
                result = await database_create("water_tank_level", water_tank_level_data) #Envia los datos de nivel de agua del tanque a la base de datos
                if result is None:
                    print("Failed to save water tank level data")


                for i in range(valveNum):
                    if soilMdata[i]["error_code"] <= 0:
                        await sensor_data_create(i) # Envia los datos de humedad del suelo y estado de la válvula a la base de datos


                print("Data sent to database")

            except Exception as e:
                print(f"Error in database sync: {e}")
            


async def sensor_data_create(i): # Crea un fomato con los datos del sensor de humedad del suelo y el estado de la válvula
    
    # Format data for database insert
    sensor_data = {
        "sensor_id": i+1,
        "soilM_sensor": soilMdata[i]["data"],
        "valve_state": valvedata[i]["state"]
    }
    
    # Send Relative humidity data to database
    result = await database_create(f"{i+1:02}_soilM_and_valve_data", sensor_data)
    if result is None:
        print("Failed to save Relative humidity data")
    return


async def download_data_database(): #descarga presets de la base de datos y los guarda en presetsdata

    # await asyncio.sleep(2)
    data = await database_getFullList("presets")
    
    for i in range (valveNum):
        presetsdata.append(data[i])

        namedata[i]["name"] = presetsdata[i].name
        soilMdata[i]["low_moist_limit"] = presetsdata[i].low_moist_limit
        soilMdata[i]["high_moist_limit"] = presetsdata[i].high_moist_limit

    print("Data read from database")
    return 


async def update_data_database(i):  # Actualiza los datos de presetsdata en la base de datos
    # Format data for database insert
    presets_data = {
        "sensor_id": i+1,
        "name": namedata[i]["name"],
        "low_moist_limit": soilMdata[i]["low_moist_limit"],
        "high_moist_limit": soilMdata[i]["high_moist_limit"]
    }
    
    # Send temperature data to database
    record_id = presetsdata[i].id
    await database_update("presets", record_id, presets_data)
    print("Data sent to database")
    return


async def send_log_database(id, description): # Enviar logs a la base de datos a la colección logs

    if not pocketbasedata[0]["state"]:
        return
    
    # Format data for database insert
    log_data = {
        "log_id": id,
        "description": description
    }
    
    # Send logs data to database
    result = await database_create("logs", log_data)
    if result is None:
        print("Failed to save log data", log_data)
    return


async def send_water_data_database():
    send_water_data_database()
    return



def Bnuy():
      
            #  /)  /)
            # ( •-•) <(https://youtu.be/Jb9caDRp_30?si=0Gjo8C7aXNryNGFE)
            # /づづ
     
    return
