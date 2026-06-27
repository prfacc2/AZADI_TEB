#!/usr/bin/env python3
import asyncio
from playwright.async_api import async_playwright
URL = "http://127.0.0.1:8799/index.html?section=1"
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={"width":1400,"height":880})
        await pg.goto(URL, wait_until="networkidle")
        await pg.wait_for_selector("#paper")
        await pg.wait_for_timeout(700)
        # apply a reception template
        await pg.click("#btnTemplates"); await pg.wait_for_timeout(300)
        await pg.evaluate("document.querySelectorAll('.tpl-card')[0].click()")
        await pg.wait_for_timeout(500)
        await pg.screenshot(path="/home/user/webapp/docs/designer_main.png")
        # gallery
        await pg.click("#btnTemplates"); await pg.wait_for_timeout(400)
        await pg.screenshot(path="/home/user/webapp/docs/designer_gallery.png")
        # select an item -> inspector
        await pg.evaluate("document.querySelector('#tplClose').click()")
        await pg.wait_for_timeout(200)
        await pg.evaluate("var it=window.AZDesigner.S.design.items.find(i=>i.type==='field'); if(it){window.AZDesigner.S.selId=it.id;} ")
        await pg.evaluate("document.querySelector('.rp-tab[data-tab=inspector]').click()")
        await pg.evaluate("window.AZDesigner.render()")
        await pg.wait_for_timeout(300)
        await pg.screenshot(path="/home/user/webapp/docs/designer_inspector.png")
        await b.close()
        print("shots done")
asyncio.run(main())
