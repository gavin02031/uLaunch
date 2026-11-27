"""Idempotent style-only patch for the pinned Plutonium dependency."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/"libs/Plutonium/Plutonium"
def edit(path,changes):
    p=root/path
    text=p.read_text(encoding="utf-8")
    marker="// GlassNX widget material v1"
    if marker in text:return
    for old,new in changes:
        if old not in text:raise SystemExit("GlassNX widget patch anchor missing: "+str(p))
        text=text.replace(old,new)
    p.write_text(marker+"\n"+text,encoding="utf-8")
edit("include/pu/ui/ui_Dialog.hpp",[
    ("return this->dialog_clr.WithAlpha(alpha);","return this->dialog_clr.WithAlpha(static_cast<u8>(static_cast<u32>(this->dialog_clr.a)*alpha/255));"),
    ("return this->over_clr.WithAlpha(alpha);","return this->over_clr.WithAlpha(static_cast<u8>(static_cast<u32>(this->over_clr.a)*alpha/255));"),
])
edit("source/pu/ui/ui_Dialog.cpp",[
    ("drawer->RenderRoundedRectangleFill(dialog_clr, dialog_x, dialog_y, dialog_width, dialog_height, this->dialog_border_radius);",
     """drawer->RenderRoundedRectangleFill({8,14,30,static_cast<u8>(initial_fade_alpha*65/255)}, dialog_x, dialog_y+8, dialog_width, dialog_height, this->dialog_border_radius);
                drawer->RenderRoundedRectangleFill(dialog_clr, dialog_x, dialog_y, dialog_width, dialog_height, this->dialog_border_radius);
                drawer->RenderRoundedRectangle({223,237,255,static_cast<u8>(initial_fade_alpha*145/255)}, dialog_x, dialog_y, dialog_width, dialog_height, this->dialog_border_radius);
                if(dialog_width>8 && dialog_height>8)
                    drawer->RenderRoundedRectangle({250,252,255,static_cast<u8>(initial_fade_alpha*45/255)}, dialog_x+3, dialog_y+3, dialog_width-6, dialog_height-6, this->dialog_border_radius);"""),
])
edit("source/pu/ui/elm/elm_Menu.cpp",[
    ("drawer->RenderRectangleFill(this->items_clr, x, cur_item_y, this->w, this->items_h);","drawer->RenderRoundedRectangleFill(this->items_clr, x+5, cur_item_y+5, this->w-10, this->items_h-10, 24);"),
    ("drawer->RenderRectangleFill(focus_clr, x, cur_item_y, this->w, this->items_h);","drawer->RenderRoundedRectangleFill(focus_clr, x+5, cur_item_y+5, this->w-10, this->items_h-10, 24);"),
    ("drawer->RenderRectangleFill(this->items_focus_clr, x, cur_item_y, this->w, this->items_h);","drawer->RenderRoundedRectangleFill(this->items_focus_clr, x+5, cur_item_y+5, this->w-10, this->items_h-10, 24);\n                        drawer->RenderRoundedRectangle({231,242,255,150}, x+5, cur_item_y+5, this->w-10, this->items_h-10, 24);"),
])
print("GlassNX widget style ready")
