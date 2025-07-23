from fastapi import FastAPI
from pydantic import BaseModel, Field

# import all you need from fastapi-pagination
from fastapi_pagination import Page, add_pagination, paginate

app = FastAPI()  # create FastAPI app
add_pagination(app)  # important! add pagination to your app


class UserOut(BaseModel):  # define your model
    name: str = Field(..., example="Steve")
    surname: str = Field(..., example="Rogers")


users = [  # create some data
    UserOut(name="Steve", surname="Rogers"),
    # ...
]


# req: GET /users
@app.get("/users")
async def get_users() -> Page[UserOut]:
    # use Page[UserOut] as return type annotation
    return paginate(users)  # use paginate function to paginate your data