import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import json

# ===============================
# ======== CORE ENGINE ==========
# ===============================

# ===============================
# ======== CORE ENGINE ==========
# ===============================

class OutputField:
    def __init__(self, name, lsb, msb=None, labels=None, onehot=False):
        self.name = name
        self.lsb = lsb
        self.msb = msb if msb is not None else lsb
        self.width = self.msb - self.lsb + 1
        self.labels = labels
        self.onehot = onehot

        if labels and not onehot:
            max_val = (1 << self.width) - 1
            for val in labels.values():
                if val > max_val:
                    raise ValueError(f"Value too large for field {name}")


class InputEntry:
    def __init__(self, pla, input_value):
        self.pla = pla
        self.input_value = input_value
        self.output_bits = [0] * pla.output_width
        pla.entries[input_value] = self

    def set(self, name, value):
        field = self.pla.fields[name]

        # --- ONE HOT FIELD ---
        if field.onehot:
            # clear all bits
            for bit in field.labels.values():
                self.output_bits[bit] = 0

            if value in field.labels:
                self.output_bits[field.labels[value]] = 1
            return

        # --- LABELED MULTIBIT ---
        if field.labels:
            if isinstance(value, str):
                value = field.labels[value]
            for i in range(field.width):
                self.output_bits[field.lsb + i] = (value >> i) & 1
        else:
            # single bit
            self.output_bits[field.lsb] = 1 if value else 0

    def get_field_value(self, field):

        if field.onehot:
            for label, bit in field.labels.items():
                if self.output_bits[bit]:
                    return label
            return None

        if field.labels:
            value = 0
            for i in range(field.width):
                value |= (self.output_bits[field.lsb + i] << i)
            return value

        return self.output_bits[field.lsb]

    def get_output_binary(self):
        return ''.join(str(self.output_bits[i]) for i in reversed(range(self.pla.output_width)))

    def get_input_binary(self):
        return format(self.input_value, f'0{self.pla.input_width}b')


class PLA:
    def __init__(self, input_width, output_width):
        self.input_width = input_width
        self.output_width = output_width
        self.fields = {}
        self.entries = {}

    def register_bit(self, name, bit):
        self.fields[name] = OutputField(name, bit)

    def register_range(self, name, lsb, msb, labels):
        self.fields[name] = OutputField(name, lsb, msb, labels)

    def register_onehot(self, name, label_bit_dict):
        """
        label_bit_dict example:
        {
            "Carry": 0,
            "Zero": 1,
            "Negative": 2
        }
        """
        self.fields[name] = OutputField(
            name=name,
            lsb=0,
            msb=0,
            labels=label_bit_dict,
            onehot=True
        )

    def create_entry(self, input_value):
        return InputEntry(self, input_value)

class PLA_GUI:
    def __init__(self, root, plas: dict):
        self.root = root
        self.plas = plas
        self.comments = {}
        self.current_entries = {}
        self.selected_opcode = None
        self.dark_mode = False

        root.title("PLA Microcode Project Editor")

        main = ttk.Frame(root)
        main.pack(fill="both", expand=True)

        # Split layout
        left = ttk.Frame(main)
        left.pack(side="left", fill="y")

        right = ttk.Frame(main)
        right.pack(side="right", fill="both", expand=True)

        # =========================
        # LEFT PANEL (Opcode List)
        # =========================

        ttk.Label(left, text="Opcodes").pack()

        self.search_var = tk.StringVar()
        self.search_var.trace_add("write", self.refresh_opcode_list)

        ttk.Entry(left, textvariable=self.search_var).pack(fill="x", padx=5)

        self.opcode_listbox = tk.Listbox(left, height=25)
        self.opcode_listbox.pack(fill="y", expand=True, padx=5)
        self.opcode_listbox.bind("<<ListboxSelect>>", self.select_opcode)

        ttk.Button(left, text="Delete", command=self.delete_opcode).pack(fill="x", padx=5, pady=2)
        ttk.Button(left, text="Duplicate", command=self.duplicate_opcode).pack(fill="x", padx=5, pady=2)

        # =========================
        # RIGHT PANEL (Editor)
        # =========================

        top = ttk.Frame(right)
        top.pack(fill="x")

        ttk.Label(top, text="Input (binary):").pack(side="left")

        self.input_var = tk.StringVar()
        ttk.Entry(top, textvariable=self.input_var, width=10).pack(side="left")

        ttk.Button(top, text="Load/Create", command=self.load_entries).pack(side="left")

        ttk.Button(top, text="Save Project", command=self.save_project).pack(side="right")
        ttk.Button(top, text="Load Project", command=self.load_project).pack(side="right")
        ttk.Button(top, text="Export PLAs", command=self.export_plas).pack(side="right")
        ttk.Button(top, text="Toggle Dark Mode", command=self.toggle_dark_mode).pack(side="right")
        # Comment
        comment_frame = ttk.Frame(right)
        comment_frame.pack(fill="x", pady=5)

        ttk.Label(comment_frame, text="Comment:").pack(side="left")
        self.comment_var = tk.StringVar()
        self.comment_var.trace_add("write", self.update_comment)
        ttk.Entry(comment_frame, textvariable=self.comment_var).pack(fill="x", expand=True, side="left")

        # Scroll area
        canvas = tk.Canvas(right)
        scrollbar = ttk.Scrollbar(right, command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)

        scrollbar.pack(side="right", fill="y")
        canvas.pack(side="left", fill="both", expand=True)

        self.controls_frame = ttk.Frame(canvas)
        canvas.create_window((0,0), window=self.controls_frame, anchor="nw")
        self.controls_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

        self.canvas = canvas
        self.enable_trackpad_scrolling()
        self.widgets = {}
        self.build_controls()

    # =========================
    # Opcode List Management
    # =========================

    def refresh_opcode_list(self, *args):
        self.opcode_listbox.delete(0, tk.END)
        search = self.search_var.get().lower()

        for val in sorted(self.get_all_input_values()):
            comment = self.comments.get(val, "")
            display = f"{format(val,'06b')}  |  {comment}"
            if search in comment.lower():
                self.opcode_listbox.insert(tk.END, display)

    def get_all_input_values(self):
        vals = set()
        for pla in self.plas.values():
            vals.update(pla.entries.keys())
        return vals

    def select_opcode(self, event):
        if not self.opcode_listbox.curselection():
            return
        text = self.opcode_listbox.get(self.opcode_listbox.curselection())
        binary = text.split("|")[0].strip()
        self.input_var.set(binary)
        self.load_entries()

    def delete_opcode(self):
        try:
            val = int(self.input_var.get(), 2)
        except:
            return
        for pla in self.plas.values():
            pla.entries.pop(val, None)
        self.comments.pop(val, None)
        self.current_entries.clear()
        self.refresh_opcode_list()

    def duplicate_opcode(self):
        try:
            val = int(self.input_var.get(), 2)
        except:
            return

        new_val = val
        while new_val in self.get_all_input_values():
            new_val += 1

        for name, pla in self.plas.items():
            if val in pla.entries:
                old = pla.entries[val]
                new = pla.create_entry(new_val)
                new.output_bits = old.output_bits.copy()

        self.comments[new_val] = self.comments.get(val, "") + " (copy)"
        self.refresh_opcode_list()

    # =========================
    # Load / Edit Entries
    # =========================

    def load_entries(self):
        try:
            val = int(self.input_var.get(), 2)
        except:
            messagebox.showerror("Error","Invalid binary")
            return

        self.selected_opcode = val
        self.current_entries.clear()

        for name, pla in self.plas.items():
            if val not in pla.entries:
                pla.create_entry(val)
            self.current_entries[name] = pla.entries[val]

        self.comment_var.set(self.comments.get(val,""))
        self.refresh_controls()
        self.refresh_opcode_list()
        self.validate_duplicate_alu()

    def refresh_controls(self):
        for name, pla in self.plas.items():
            entry = self.current_entries[name]
            for field_name, field in pla.fields.items():
                var = self.widgets[name][field_name]
                var.set(entry.get_field_value(field))

    def update_comment(self,*args):
        if self.selected_opcode is not None:
            self.comments[self.selected_opcode] = self.comment_var.get()
            self.refresh_opcode_list()

    # =========================
    # Duplicate ALU Validation
    # =========================

    def validate_duplicate_alu(self):
        if "Execute Control" not in self.plas:
            return

        current = self.plas["Execute Control"].entries.get(self.selected_opcode)
        if not current:
            return

        current_bits = current.output_bits

        # for val, entry in self.plas["Execute Control"].entries.items():
        #     if val != self.selected_opcode and entry.output_bits == current_bits:
        #         messagebox.showwarning("Duplicate ALU config",
        #             f"ALU config identical to opcode {format(val,'06b')}")
        #         return

    # =========================
    # Controls Build
    # =========================

    def build_controls(self):
        for widget in self.controls_frame.winfo_children():
            widget.destroy()

        for pla_name, pla in self.plas.items():
            frame = ttk.LabelFrame(self.controls_frame, text=pla_name)
            frame.pack(fill="x", padx=5, pady=5)

            self.widgets[pla_name] = {}

            for field_name, field in pla.fields.items():
                sub = ttk.LabelFrame(frame, text=field_name)
                sub.pack(fill="x", padx=5, pady=3)

                # ONE HOT
                if field.onehot:
                    var = tk.StringVar()
                    self.widgets[pla_name][field_name] = var

                    for label, bit in field.labels.items():
                        ttk.Radiobutton(
                            sub,
                            text=label,
                            variable=var,
                            value=label,
                            command=lambda p=pla_name, f=field_name: self.update_field(p, f)
                        ).pack(anchor="w")

                # MULTIBIT
                elif field.labels:
                    var = tk.IntVar()
                    self.widgets[pla_name][field_name] = var

                    for label, val in field.labels.items():
                        ttk.Radiobutton(
                            sub,
                            text=label,
                            variable=var,
                            value=val,
                            command=lambda p=pla_name, f=field_name: self.update_field(p, f)
                        ).pack(anchor="w")

                # SINGLE BIT
                else:
                    var = tk.IntVar()
                    self.widgets[pla_name][field_name] = var

                    ttk.Checkbutton(
                        sub,
                        text="Enable",
                        variable=var,
                        command=lambda p=pla_name, f=field_name: self.update_field(p, f)
                    ).pack(anchor="w")

    def update_field(self, pla_name, field_name):
        entry = self.current_entries.get(pla_name)
        if not entry:
            return
        entry.set(field_name, self.widgets[pla_name][field_name].get())
        self.validate_duplicate_alu()

    # =========================
    # Save / Load Project
    # =========================

    def save_project(self):
        file = filedialog.asksaveasfilename(defaultextension=".opcodes")
        if not file:
            return

        data = {"opcodes":[]}

        for val in sorted(self.get_all_input_values()):
            op = {
                "input": val,
                "comment": self.comments.get(val,""),
                "plas":{}
            }
            for name,pla in self.plas.items():
                if val in pla.entries:
                    op["plas"][name] = pla.entries[val].output_bits
            data["opcodes"].append(op)

        with open(file,"w") as f:
            json.dump(data,f,indent=4)

    def load_project(self):
        file = filedialog.askopenfilename(filetypes=[("Opcode Project","*.opcodes")])
        if not file:
            return

        with open(file,"r") as f:
            data=json.load(f)

        for pla in self.plas.values():
            pla.entries.clear()
        self.comments.clear()

        for op in data["opcodes"]:
            val = op["input"]
            for name,bits in op["plas"].items():
                entry = self.plas[name].create_entry(val)
                entry.output_bits = bits
            self.comments[val] = op["comment"]

        self.refresh_opcode_list()

    # =========================
    # Export PLA Text Files
    # =========================

    def export_plas(self):
        directory = filedialog.askdirectory()
        if not directory:
            return

        for name,pla in self.plas.items():
            with open(f"{directory}/{name}.txt","w") as f:
                for val in sorted(pla.entries.keys()):
                    entry=pla.entries[val]
                    # comment=self.comments.get(val,"")
                    line=f"{entry.get_input_binary()} {entry.get_output_binary()}"
                    # if comment:
                    #     line+=" ; "+comment
                    f.write(line+"\n")


    def enable_trackpad_scrolling(self):
        # Only scroll when mouse is over the canvas
        self.canvas.bind("<Enter>", self._bind_scroll)
        self.canvas.bind("<Leave>", self._unbind_scroll)


    def _bind_scroll(self, event=None):
        # macOS + Windows
        self.canvas.bind_all("<MouseWheel>", self._on_mousewheel)

        # Linux fallback
        self.canvas.bind_all("<Button-4>", self._on_mousewheel_linux)
        self.canvas.bind_all("<Button-5>", self._on_mousewheel_linux)


    def _unbind_scroll(self, event=None):
        self.canvas.unbind_all("<MouseWheel>")
        self.canvas.unbind_all("<Button-4>")
        self.canvas.unbind_all("<Button-5>")


    def _on_mousewheel(self, event):
        # macOS trackpad sends small delta values
        self.canvas.yview_scroll(int(-event.delta/2), "units")


    def _on_mousewheel_linux(self, event):
        if event.num == 4:
            self.canvas.yview_scroll(-20, "units")
        elif event.num == 5:
            self.canvas.yview_scroll(20, "units")

    def toggle_dark_mode(self):
        self.dark_mode = not self.dark_mode
        style = ttk.Style()

        if self.dark_mode:
            bg = "#1e1e1e"
            fg = "#ffffff"
            field = "#2a2a2a"
            list_bg = "#252526"
        else:
            bg = "#f0f0f0"
            fg = "#000000"
            field = "#ffffff"
            list_bg = "#ffffff"

        # force a theme that allows overrides
        style.theme_use("default")

        # root
        self.root.configure(bg=bg)

        # base ttk styles
        style.configure(".", background=bg, foreground=fg)

        style.configure("TFrame", background=bg)
        style.configure("TLabel", background=bg, foreground=fg)
        style.configure("TLabelFrame", background=bg, foreground=fg)
        style.configure("TLabelFrame.Label", background=bg, foreground=fg)

        style.configure("TButton", background=field, foreground=fg)
        style.configure("TCheckbutton", background=bg, foreground=fg)
        style.configure("TRadiobutton", background=bg, foreground=fg)

        style.configure("TEntry",
            fieldbackground=field,
            foreground=fg
        )

        # entry cursor/text
        style.map("TEntry",
            foreground=[("disabled", fg), ("!disabled", fg)]
        )

        # widgets not using ttk styling
        self.opcode_listbox.configure(bg=list_bg, fg=fg)
        self.canvas.configure(bg=bg)

# ===============================
# ====== YOUR PLA SETUP =========
# ===============================

# (Your build_* functions unchanged below)
# ===============================
# ====== ALU EXAMPLE SETUP ======
# ===============================

def build_alu_control_pla():
    pla = PLA(input_width=6, output_width=14)

    pla.register_bit("rotate_mode", 0)
    pla.register_bit("cin_enable", 1)

    pla.register_range("carry_flag_select", 2, 3, {
        "ADDER_COUT": 0,
        "NOT_USED": 1,
        "SHIFT_R_OVERFLOW": 2,
        "SHIFT_L_OVERFLOW": 3
    })

    pla.register_bit("carry_flag_enable", 4)
    pla.register_bit("overflow_flag_enable", 5)
    pla.register_bit("negative_flag_enable", 6)

    pla.register_range("overflow_flag_select", 7, 8, {
        "ADDER_COUT": 0,
        "COUT_XOR_MSB": 1,
        "SIGNED_OVERFLOW": 2,
        "MUL_UPPER_NOT_ZERO": 3
    })

    pla.register_range("result_select", 9, 11, {
        "ADD": 0,
        "RIGHT_SHIFT": 1,
        "OR": 2,
        "AND": 3,
        "MUL_LOWER": 4,
        "MUL_UPPER": 5,
        "XOR": 6,
        "LEFT_SHIFT": 7
    })

    pla.register_bit("adder_cin", 12)
    pla.register_bit("invert_adder_b", 13)

    return pla

def build_read_control_pla():
    pla = PLA(input_width=6, output_width=11)

    pla.register_bit("mute EXE forwarding", 0)
    pla.register_bit("mute MEM forwarding", 1)
    pla.register_bit("inject instruction", 2)
    pla.register_bit("16bit path A allow data", 3)
    pla.register_bit("16bit path A pull immediate from instruction stream", 4)
    pla.register_bit("16bit path B allow data", 5)
    pla.register_bit("16bit path B pull immediate from instruction stream", 6)
    pla.register_bit("two first field write select automatic mode enable", 7)
    pla.register_bit("two field two to path B (otherwise normal field 3)", 8)
    pla.register_bit("normal field 3 to path A (otherwise normal field 2)", 8)
    pla.register_range("r0-r15 write field select", 10, 11, {
        "Normal field 1" : 0,
        "Two field 1 (only r0-r15)" : 1,
        "Normal field 2" : 2
    })
    
    return pla


def build_execute_control_pla():
    pla = PLA(input_width=6, output_width=19)

    pla.register_onehot("32 bit Memory addrews Path assert", {
        "32_BIT_AGU_ASSERT_MEMORY_PATH" : 0,
        "32_BIT_PATH_ASSERT_MEMORY_PATH" : 1
    })
    pla.register_onehot("32 bit main path assert", {
        "16_BIT_PATH_A_ASSERT_HIGH_32_BIT_MAIN_PATH" : 2,
        "32_BIT_AGU_ASSERT_MAIN_PATH" : 3,
        "32_BIT_PATH_ASSERT_MAIN_PATH" : 4,
        "16_BIT_PATH_A_ASSERT_LOW_32_BIT_MAIN_PATH" : 5,
        "16_BIT_PATH_B_ASSERT_HIGH_32_BIT_MAIN_PATH" : 6,
        "16_BIT_PATH_B_ASSERT_LOW_32_BIT_MAIN_PATH" : 7
    })

    pla.register_onehot("16 bit path assert", {
        "FLAGS_ASSERT_16_BIT_RESULT" : 8,
        "ALU_ASSERT_16_BIT_RESULT" : 9,
        "16_BIT_PATH_B_ASSERT_16_BIT_RESULT" : 10,
        "16_BIT_PATH_A_ASSERT_16_BIT_RESULT" : 11
    })

    pla.register_range("ALU_CONTROL", 12, 15, {
        "ADD" : 0,
        "ADD with carry" : 1,
        "SUB" : 2,
        "SUB with carry" : 3,
        "Multiply lower" : 4,
        "Multiply upper" : 5,
        "Shift left" : 6,
        "Shift right" : 7,
        "Rotate left" : 8,
        "Rotate right" : 9,
        "AND" : 10,
        "OR" : 11,
        "XOR" : 12,
        "NOT" : 13,
    })
    pla.register_bit("AGU_SUBTRACT_MODE", 16)
    pla.register_bit("ALU_FLAGS_WRITE_ENABLE", 17)
    pla.register_bit("Allow 16 bit path A to 32 bit memory path side flip opposite to automatic 32bit reg half", 18)



    return pla

def build_writeback_control_pla():
    pla = PLA(input_width=6, output_width=3)

    pla.register_bit("16_BIT_REGISTERS_WRITE_ENABLE", 0)
    pla.register_bit("32_BIT_REGISTERS_WRITE_SELECT_ENABLE", 1)
    pla.register_bit("SPECIAL_REGISTERS_WRITE_SELECT_ENABLE", 2)

    return pla

def build_memory_control_pla():
    pla = PLA(input_width=6, output_width=6)

    pla.register_bit("Enable memory or cache state operation", 0)
    pla.register_bit("Two byte data operation", 1)
    pla.register_bit("Data mode write (otherwise data mode read)", 2)
    pla.register_onehot("16 Bit result", {
        "Memory result" : 3,
        "EXE result" : 4
    })
    pla.register_bit("Data operation (r/w) enable", 5)

    return pla
# ===============================
# ========= MAIN RUN ============
# ===============================

root = tk.Tk()
alupla = build_alu_control_pla()
readpla = build_read_control_pla()
exepla = build_execute_control_pla()
mempla = build_memory_control_pla()
wbpla = build_writeback_control_pla()
gui = PLA_GUI(root, {
    "Read and Decode control" : readpla,
    "Execute Control" : exepla,
    "Memory Control" : mempla,
    "Writeback Control" : wbpla
})
# gui2 = PLA_GUI(root, {"ALU Control":alupla})
root.mainloop()