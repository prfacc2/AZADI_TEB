#!/usr/bin/env python3
import asyncio, json
from playwright.async_api import async_playwright

URL = "http://127.0.0.1:8799/index.html?section=1"

async def main():
    errors, warns, logs = [], [], []
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page()
        pg.on("console", lambda m: (warns if m.type == "warning" else logs).append(m.text))
        pg.on("pageerror", lambda e: errors.append(str(e)))
        await pg.goto(URL, wait_until="networkidle")
        await pg.wait_for_selector("#paper", timeout=5000)
        await pg.wait_for_timeout(800)

        # 1) templates loaded?
        ntpl = await pg.evaluate("(window.AZ_TEMPLATES||[]).length")
        print("templates:", ntpl)

        # 2) open gallery, count cards per tab
        await pg.click("#btnTemplates")
        await pg.wait_for_selector(".tpl-grid", timeout=3000)
        recep = await pg.eval_on_selector_all(".tpl-card", "els=>els.length")
        print("reception cards:", recep)
        # switch to appointment
        await pg.evaluate("document.querySelectorAll('.tpl-tab')[1].click()")
        await pg.wait_for_timeout(200)
        appt = await pg.eval_on_selector_all(".tpl-card", "els=>els.length")
        print("appointment cards:", appt)
        await pg.evaluate("document.querySelectorAll('.tpl-tab')[2].click()")
        await pg.wait_for_timeout(200)
        lab = await pg.eval_on_selector_all(".tpl-card", "els=>els.length")
        print("lab cards:", lab)
        # apply first lab template
        await pg.evaluate("document.querySelectorAll('.tpl-card')[0].click()")
        await pg.wait_for_timeout(400)
        nitems = await pg.evaluate("window.AZDesigner.S.design.items.length")
        print("applied template items:", nitems)

        # 3) add a field from palette
        await pg.evaluate("document.querySelector('.rp-tab[data-tab=palette]').click()")
        await pg.wait_for_timeout(200)
        await pg.evaluate("document.querySelector('#paletteList .pl-field').click()")
        await pg.wait_for_timeout(300)
        nitems2 = await pg.evaluate("window.AZDesigner.S.design.items.length")
        print("after add field:", nitems2)

        # 4) SAVE — capture the /api/save response by hooking fetch result via JS promise
        save_result = await pg.evaluate("""async () => {
            return await new Promise(res => {
                const xhr = new XMLHttpRequest();
                xhr.open('POST','api/save',true);
                xhr.setRequestHeader('Content-Type','application/json;charset=utf-8');
                xhr.onreadystatechange=function(){ if(xhr.readyState===4){
                    try{ res(JSON.parse(xhr.responseText)); }catch(e){ res({err:String(e)}); } } };
                xhr.send(JSON.stringify({design: window.AZDesigner.S.design}));
            });
        }""")
        print("SAVE result:", json.dumps(save_result, ensure_ascii=False))

        # 5) trigger the actual UI save button too
        await pg.evaluate("window.AZDesigner.S.design.name='تست ذخیره'; window.AZDesigner.S.design.kind='user';")
        await pg.click("#btnSave")
        await pg.wait_for_timeout(700)
        toast = await pg.eval_on_selector("#toast", "el=>el.textContent")
        print("toast after save:", toast)

        await b.close()

    print("\n--- console warnings ---")
    for w in warns: print("WARN:", w)
    print("--- page errors ---")
    for e in errors: print("ERR:", e)
    sync = [w for w in warns if "Synchronous XMLHttpRequest" in w or "timeout attribute" in w]
    print("\nSYNC-XHR warnings:", len(sync))
    ok = (ntpl >= 60 and recep == 20 and appt == 20 and lab == 20 and
          save_result.get("ok") is True and len(sync) == 0 and len(errors) == 0)
    print("\nRESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1

import sys
sys.exit(asyncio.run(main()))
