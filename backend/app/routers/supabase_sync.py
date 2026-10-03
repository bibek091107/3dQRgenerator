from fastapi import APIRouter, HTTPException
from ..supabase_client import check_exists, SUPABASE_URL, SUPABASE_KEY
import httpx

router = APIRouter(prefix="/api/supabase", tags=["supabase"])

def get_headers():
    return {
        "apikey": SUPABASE_KEY,
        "Authorization": f"Bearer {SUPABASE_KEY}",
        "Content-Type": "application/json",
    }

@router.get("/asset/{qr_type}/{item_id}")
async def get_asset(qr_type: str, item_id: str):
    if not SUPABASE_URL or not SUPABASE_KEY:
        raise HTTPException(status_code=500, detail="Missing Supabase credentials")
    
    table = "tools" if qr_type.lower() == "tool" else "holders"
    id_col = "tool_id" if table == "tools" else "holder_id"
    url = f"{SUPABASE_URL}/rest/v1/{table}?{id_col}=eq.{item_id}"
    
    async with httpx.AsyncClient() as client:
        response = await client.get(url, headers=get_headers())
        if response.status_code == 200:
            data = response.json()
            if len(data) > 0:
                return data[0]
            raise HTTPException(status_code=404, detail="Not found")
        raise HTTPException(status_code=500, detail="Supabase query failed")
