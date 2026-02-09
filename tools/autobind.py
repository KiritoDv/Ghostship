import re
import sys
import argparse
from enum import Enum
from pathlib import Path

# --- Configuration ---

PREPROCESSOR_DEFINES = {
    'VERSION_US', 
    'ENABLE_AUDIO',
    'NON_MATCHING',
}

TYPE_MAP = {
    'uint8_t': 'number', 'u8': 'number', 'int8_t': 'number', 's8': 'number',
    'uint16_t': 'number', 'u16': 'number', 'int16_t': 'number', 's16': 'number',
    'uint32_t': 'number', 'u32': 'number', 'int32_t': 'number', 's32': 'number',
    'uint64_t': 'number', 'u64': 'number', 'int64_t': 'number', 's64': 'number',
    'size_t': 'number', 'uintptr_t': 'number',
    'f32': 'number', 'f64': 'number', 'float': 'number', 'double': 'number',
    'int': 'number', 'unsigned': 'number',
    'bool': 'boolean',
    'char': 'string', 'const char*': 'string',
    'void': 'nil',
    '...': 'any',
    'CONTROLLERBUTTONS_T': 'number'
}

BLACKLIST_TERMS = {
    'ast_audio', 'sf64audio_provisional', 'audioseq_cmd', 'audiothread_cmd',
    'libaudio', 'portable-file-dialogs', 'rmonint.h', 'PR/', 'libultra/',
    'libc/', 'dbgproto', 'prevent', 'piint', 'siint', 'sf64dma', 'osint',
    'FrameInterpolation', 'mods.h', 'include/level_table.h', 'dr_mp3.h',
    'goddard', 'importer', 'EventSystem', 'port/ui', 'port/mods',
    'port/interpolation', 'port/game', 'port/data', 'port/console', 'port/audio',
    'CoreMath', 'Matrix', '_sh', 'GameOverlay'
}

BLACKLIST_SYMBOLS = {
    'va_list',
    'OS_Internal_',
    '__attribute__',
    'unknown_struct',
    '_eu_',
    '_sh_',
    'debug_',
    'eu_'
}

class OutputType(Enum):
    LUA = 1
    CPP = 2

# --- Helpers ---

class OutputBuffer:
    """Handles writing to list (CPP) or stdout (Lua) transparently."""
    def __init__(self, mode: OutputType):
        self.mode = mode
        self.buffer = []

    def write(self, text: str):
        if self.mode == OutputType.LUA:
            print(text)
        else:
            self.buffer.append(str(text))

    def get_content(self):
        return self.buffer

class BindingGenerator:
    def __init__(self, mode: OutputType, root_dir: str = "."):
        self.mode = mode
        self.root_dir = Path(root_dir)
        self.event_list = []
        
        # Buffers for CPP mode
        self.bufs = {
            'structs': OutputBuffer(mode),
            'externs': OutputBuffer(mode),
            'enums': OutputBuffer(mode),
            'events': OutputBuffer(mode)
        }
        
        # Regex Patterns
        self.re_enum = re.compile(r"enum\s+(\w+)\s*(?:\s*:\s*(\w+))?[\s\n\r]*\{")
        self.re_struct = re.compile(r"struct\s+(\w+)\s*(?:\s*:\s*(\w+))?[\s\n\r]*\{")
        
        # Allow alphanumerics, =, _, ., -, and whitespace for cleaning lines
        self.re_clean = re.compile(r'(/\*.*?\*/)|(//.*$)|([^a-zA-Z0-9=_\-\.\s])')
        self.re_comment = re.compile(r'(/\*.*?\*/)')

    def sanitize_type(self, type_str: str) -> str:
        """Converts C types to Lua types."""
        if self.mode == OutputType.CPP:
            return type_str.strip()

        clean_type = type_str.strip().replace('*', '').replace('&', '').replace('const', '').strip()
        return TYPE_MAP.get(clean_type, clean_type)

    def is_blacklisted(self, filepath: Path) -> bool:
        path_str = str(filepath)
        return any(term in path_str for term in BLACKLIST_TERMS)

    def read_and_filter_lines(self, file_path: Path):
        """
        Reads a file and filters out:
        1. Blocks wrapped in start-sol:ignore / end-sol:ignore
        2. Lines containing words from BLACKLIST_SYMBOLS
        3. Code disabled by #ifdef / #ifndef based on PREPROCESSOR_DEFINES
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                raw_lines = f.readlines()
        except IOError:
            return []

        filtered_lines = []
        
        # 1. Manual Ignore Block State
        ignoring_manual = False
        
        # 2. Preprocessor State
        # Stack of booleans. If ALL are True, we process the line.
        # We start with [True] so the global scope is active.
        ifdef_stack = [True] 

        # Pre-calculate lowercase sets for speed
        lower_blacklist = {w.lower() for w in BLACKLIST_SYMBOLS}
        active_macros = PREPROCESSOR_DEFINES

        for line in raw_lines:
            stripped = line.strip()

            # --- Preprocessor Logic ---
            if stripped.startswith('#'):
                # Split "#ifdef MACRO" -> ["#ifdef", "MACRO"]
                parts = stripped.split()
                if not parts: continue
                directive = parts[0]

                if directive == '#ifdef':
                    macro = parts[1] if len(parts) > 1 else ""
                    # Push True if defined, False if not
                    ifdef_stack.append(macro in active_macros)
                    continue
                
                elif directive == '#ifndef':
                    macro = parts[1] if len(parts) > 1 else ""
                    # Push True if NOT defined
                    ifdef_stack.append(macro not in active_macros)
                    continue

                elif directive == '#else':
                    # Invert the current block's state
                    if len(ifdef_stack) > 1:
                        ifdef_stack[-1] = not ifdef_stack[-1]
                    continue

                elif directive == '#endif':
                    # Pop the current state
                    if len(ifdef_stack) > 1:
                        ifdef_stack.pop()
                    continue
                
                # Handle basic #if 0 (Common for commenting out code)
                elif directive == '#if':
                    condition = parts[1] if len(parts) > 1 else ""
                    if condition == '0':
                        ifdef_stack.append(False)
                    elif condition == '1':
                        ifdef_stack.append(True)
                    else:
                        # Complex expression? Default to TRUE to be safe, 
                        # or FALSE to be strict. (Choosing True to avoid skipping valid code)
                        ifdef_stack.append(True) 
                    continue

                # Skip other directives (#include, #pragma) but don't filter them out yet
                # (You might want to keep includes, or skip them if you want)

            # --- Decision: Should we parse this line? ---
            
            # 1. Check Preprocessor Stack (Must be all True)
            if not all(ifdef_stack):
                continue

            # 2. Check Manual Ignore Blocks
            if 'start-sol:ignore' in line:
                ignoring_manual = True
                continue
            if 'end-sol:ignore' in line:
                ignoring_manual = False
                continue
            if ignoring_manual:
                continue

            # 3. Check Blacklist Symbols
            line_content = line.split('//')[0].lower()
            if any(bad_word in line_content for bad_word in lower_blacklist):
                continue
            
            # If we survived all checks, add the line
            filtered_lines.append(line)
                
        return filtered_lines

    def write_lua_header(self):
        print("""
Game = {}
Assets = {}
UIWidgets = {}
Events = {}
---@alias ListenerID number
---@class Asset
local Asset = {}
function Asset:Register() end
---@return integer
function Asset:Load8() end
---@return integer
function Asset:Load16() end
---@return integer
function Asset:Load32() end
---@return Vtx
function Asset:LoadVtx() end
---@return Gfx
function Asset:LoadGfx() end
---@return string
function Asset:__tostring() end
function RegisterListener(eventId, callback, priority) end
---@class Gfx
local Gfx = {}
function gNextMasterDisp() end
function gRefMasterDisp() end
---@class Matrix
local Matrix = {}
function gRefGfxMatrix() end
function gDPSetPrimColor(m, l, r, g, b, a) end
""")

    # --- Parsers ---

    def parse_enums(self, file_path: Path, as_value=False, buffer_key='enums'):
        lines = self.read_and_filter_lines(file_path)
        if not lines: return

        out = self.bufs[buffer_key]
        in_enum = False
        enum_name = ""
        enum_data = {}
        enum_idx = 0

        for line in lines:
            line = line.strip()
            
            if not in_enum:
                match = self.re_enum.search(line)
                if match:
                    enum_name = match.group(1)
                    enum_data[enum_name] = []
                    in_enum = True
                    enum_idx = -1
                continue

            if '}' in line:
                in_enum = False
                continue

            # Clean line but preserve necessary chars
            line = self.re_clean.sub('', line).strip()
            if not line: continue

            name = ""
            val = None
            val_raw = ""

            # Strategy 1: Explicit assignment with '='
            if '=' in line:
                parts = line.split('=')
                name = parts[0].strip()
                val_raw = parts[1].strip()
            # Strategy 2: Space separated (e.g. "DIALOG_NONE -1")
            elif ' ' in line:
                parts = line.split()
                # Use LAST item as value, FIRST as name
                name = parts[0].strip()
                val_raw = parts[-1].strip()
            # Strategy 3: Just a name (Auto-increment)
            else:
                name = line
                if isinstance(enum_idx, int):
                    enum_idx += 1
                    val = enum_idx
                else:
                    # Previous was string/alias, can't math easily in Python
                    val = f"{enum_idx} + 1"
                    enum_idx = val

            # If we found a raw value string, try to parse it
            if val is None:
                try:
                    # Is it a number? (Hex 0x65 or Decimal -1)
                    val = int(val_raw, 0)
                    enum_idx = val
                except ValueError:
                    # It is an alias (e.g. special_wooden_door)
                    val = val_raw
                    enum_idx = val_raw

            enum_data[enum_name].append({'name': name, 'value': val})

        for name, values in enum_data.items():
            if self.mode == OutputType.CPP:
                out.write(f'auto enum_{name} = lua["{name}"].force();')
                for entry in values:
                    k, v = entry['name'], entry['value']
                    if any(x in k for x in ['ifdef', 'endif', 'else']): continue
                    clean_key = k.replace('EVENT_PRIORITY_', '').replace('SF64_VER_', '').replace('OBJECT_TYPE_', '')
                    
                    # If as_value is True, use literal value (v), else use C++ Key (k)
                    target = v if as_value else k
                    out.write(f'enum_{name}["{clean_key}"] = (uint32_t) {target};')
                out.write('')
            else:
                out.write(f'---@enum {name}')
                out.write(f'{name} = {{')
                for i, entry in enumerate(values):
                    k, v = entry['name'], entry['value']
                    if any(x in k for x in ['ifdef', 'endif', 'else']): continue
                    clean_key = k.replace('EVENT_PRIORITY_', '').replace('SF64_VER_', '').replace('OBJECT_TYPE_', '')
                    out.write(f'    {clean_key} = {v}{"," if i < len(values)-1 else ""}')
                out.write('}\n')

    def parse_structs(self, file_path: Path, buffer_key='structs'):
        lines = self.read_and_filter_lines(file_path)
        if not lines: return

        out = self.bufs[buffer_key]
        in_struct = False
        struct_name = ""
        structs = {}
        scope_stack = [] 

        for line in lines:
            line = line.strip()
            
            # 1. Main Struct Detection
            if not in_struct:
                match = self.re_struct.search(line)
                if match:
                    struct_name = match.group(1)
                    structs[struct_name] = []
                    in_struct = True
                continue

            # 2. Nested Scope Start
            if re.search(r'\b(struct|union)\s*\{', line):
                scope_stack.append([]) 
                continue

            # 3. Nested Scope End
            if '}' in line and scope_stack:
                clean_end = line.replace(';', '').replace('}', '').strip()
                scope_instance_name = clean_end if clean_end else None
                popped_members = scope_stack.pop()
                
                for m in popped_members:
                    if scope_instance_name:
                        m['access'] = f"{scope_instance_name}.{m['access']}"
                    
                    if scope_stack:
                        scope_stack[-1].append(m)
                    else:
                        structs[struct_name].append(m)
                continue

            # 4. Struct End
            if '}' in line:
                if not '{' in line: in_struct = False; continue

            # 5. Member Parsing
            line = self.re_comment.sub('', line).strip()
            if ';' in line:
                parts = line.split(';')
                decl = parts[0].strip()
                member = {}
                
                # --- A. Bitfields ---
                if ':' in decl and '::' not in decl: 
                    name_parts = decl.split(':')
                    m_name = name_parts[0].split()[-1]
                    member = {
                        'name': m_name, 'access': m_name, 'export': 'bitfield',
                        'type': self.sanitize_type('u8' if int(name_parts[1]) < 8 else 'u32')
                    }

                # --- B. Function Pointers ---
                elif '(' in decl:
                    if '[' in decl and ')' in decl and '*' in decl.split('(')[1]:
                         clean_name = decl.split('(*')[1].split(')')[0]
                         member = {
                             'name': clean_name, 'access': clean_name, 'export': 'variable',
                             'type': self.sanitize_type(decl.split()[0])
                         }
                    else:
                        pre_paren = decl.split('(')[0].split()
                        if len(pre_paren) >= 2:
                            m_name = pre_paren[-1]
                            member = {
                                'name': m_name, 'access': m_name, 'export': 'function',
                                'type': self.sanitize_type(pre_paren[-2])
                            }

                # --- C. Arrays ---
                elif '[' in decl:
                    pre_array = decl.split('[')[0].strip()
                    type_part, name_part = pre_array.rsplit(' ', 1)
                    stars = name_part.count('*')
                    clean_name = name_part.replace('*', '').strip()
                    full_type = type_part + ('*' * stars)
                    
                    member = {
                        'name': clean_name, 'access': clean_name, 'export': 'table',
                        'type': self.sanitize_type(full_type),
                        'dims': decl.count('[')
                    }

                # --- D. Standard Variables ---
                else: 
                    type_part, name_part = decl.rsplit(' ', 1)
                    stars = name_part.count('*')
                    clean_name = name_part.replace('*', '').strip()
                    full_type = type_part + ('*' * stars)

                    member = {
                        'name': clean_name, 'access': clean_name, 'export': 'variable',
                        'type': self.sanitize_type(full_type)
                    }
                
                if member:
                    if scope_stack:
                        scope_stack[-1].append(member)
                    elif struct_name in structs:
                        structs[struct_name].append(member)

        # Output Generation
        for s_name, members in structs.items():
            if not members: continue
            
            if self.mode == OutputType.CPP:
                out.write(f'lua.new_usertype<{s_name}>("{s_name}",')
                for i, m in enumerate(members):
                    k = m['name'].replace('*', '')
                    acc = m['access'].replace('*', '')
                    term = "," if i < len(members)-1 else ""
                    t_str = m['type'] 

                    if m['export'] == 'bitfield':
                         out.write(f'    "{k}", sol::overload([] ({s_name}& s) -> {t_str} {{ return s.{acc}; }}, [] ({s_name}& s, {t_str} v) {{ s.{acc} = v; }}){term}')
                    
                    elif m['export'] == 'table':
                        dims = m.get('dims', 1)
                        if dims == 1:
                            out.write(f'    "{k}", sol::overload([] ({s_name}& s, int i) -> {t_str} {{ return s.{acc}[i]; }}, [] ({s_name}& s, int i, {t_str} v) {{ s.{acc}[i] = v; }}){term}')
                        elif dims == 2:
                            out.write(f'    "{k}", sol::overload([] ({s_name}& s, int i, int j) -> {t_str} {{ return s.{acc}[i][j]; }}, [] ({s_name}& s, int i, int j, {t_str} v) {{ s.{acc}[i][j] = v; }}){term}')
                        else:
                            out.write(f'    // "{k}" skipped (3D+ array){term}')

                    elif m['export'] == 'variable':
                        # FIX: Check if it's a nested member (contains dot)
                        # If so, we MUST use a lambda overload, because &Class::normal.y is invalid C++.
                        if '.' in acc:
                            out.write(f'    "{k}", sol::overload([] ({s_name}& s) -> {t_str} {{ return s.{acc}; }}, [] ({s_name}& s, {t_str} v) {{ s.{acc} = v; }}){term}')
                        else:
                            # Standard members can use the faster property pointer
                            out.write(f'    "{k}", sol::property(&{s_name}::{acc}, &{s_name}::{acc}){term}')

                out.write(');\n')
            else:
                out.write(f'---@class {s_name}')
                members.sort(key=lambda x: x['export'] == 'table') 
                for m in members:
                    k = m['name'].replace('*', '')
                    t = self.sanitize_type(m['type'])
                    if m['export'] == 'table':
                        dims = m.get('dims', 1)
                        if dims == 1:
                            out.write(f'function {s_name}:{k}(index, val) end')
                        elif dims == 2:
                             out.write(f'function {s_name}:{k}(row, col, val) end')
                    else:
                        out.write(f'---@field {k} {t}')
                out.write(f'{s_name} = {{}}\n')

    def parse_externs(self, file_path: Path, namespace=None, buffer_key='externs'):
        lines = self.read_and_filter_lines(file_path)
        if not lines: return

        out = self.bufs[buffer_key]

        for line in lines:
            line = re.sub(r'\s+', ' ', line.strip())
            
            # --- 0. Special Case: ALIGN_ASSET ---
            if 'ALIGN_ASSET' in line:
                try:
                    declaration_part = line.split('[')[0].strip()
                    var_name = declaration_part.split()[-1].replace('*', '')
                    
                    if '=' in line:
                        value_part = line.split('=')[1].strip()
                        if value_part.endswith(';'): value_part = value_part[:-1]
                        value = value_part.strip()
                    else:
                        value = "nil"

                    if self.mode == OutputType.CPP:
                        out.write(f'lua["Assets"]["{var_name}"] = Asset{{ {var_name} }};')
                    else:
                        out.write('---@type Asset')
                        out.write(f'Assets.{var_name} = {value}')
                except (IndexError, ValueError):
                    pass
                continue

            # --- Filter ---
            if any(x in line for x in ['#define', 'typedef', '\\', 'OSMesg', 'Framebuffer', 'sol:ignore', 'static']):
                continue

            # --- 1. Variable Exports ---
            if line.startswith('extern') and '"C"' not in line and '(' not in line:
                content = line.replace('extern', '').replace(';', '').strip()
                if ' ' not in content: continue
                
                type_part, name_part = content.rsplit(' ', 1)
                
                pointer_level = name_part.count('*')
                clean_name = name_part.replace('*', '').split('[')[0]
                full_type = type_part + ('*' * pointer_level)

                # Analysis
                is_const = 'const' in content
                is_array = '[' in name_part
                
                # Char/String logic
                is_char_type = 'char' in type_part and not any(x in type_part for x in ['u8', 's8', 'unsigned', 'signed'])
                is_string = is_char_type and (pointer_level > 0 or is_array)

                if is_string:
                    is_array = False # Treat as Scalar String
                    
                    # If it was defined as an array (extern char name[]), handle potential decay issues
                    if '[' in name_part: 
                        is_const = True # Array variables are not assignable
                        
                        # FIX: Ensure type is char* not char, so lambda returns pointer
                        if not full_type.endswith('*'):
                            full_type += '*'

                # Output
                if is_array: 
                    if self.mode == OutputType.CPP:
                        if is_const:
                            out.write(f'lua["Game"]["{clean_name}"] = [](int i) -> {full_type}& {{ return {clean_name}[i]; }};')
                        else:
                            out.write(f'lua["Game"]["{clean_name}"] = sol::overload([](int i) -> {full_type}& {{ return {clean_name}[i]; }}, [](int i, {full_type} v) {{ {clean_name}[i] = v; }});')
                    else:
                        out.write(f'---@param index number')
                        out.write(f'---@return {self.sanitize_type(full_type)}')
                        out.write(f'function Game.{clean_name}(index) end')
                        if not is_const:
                            out.write(f'---@param index number')
                            out.write(f'---@param value {self.sanitize_type(full_type)}')
                            out.write(f'function Game.{clean_name}(index, value) end')

                else: # Scalar
                    if self.mode == OutputType.CPP:
                        if is_const:
                            out.write(f'lua["Game"]["{clean_name}"] = []() -> {full_type} {{ return {clean_name}; }};')
                        else:
                            out.write(f'lua["Game"]["{clean_name}"] = sol::overload([]() -> {full_type} {{ return {clean_name}; }}, []({full_type} v) {{ {clean_name} = v; }});')
                    else:
                        out.write(f'---@field {clean_name} {self.sanitize_type(full_type)}')
                continue

            # --- 2. Function Exports ---
            if '(' in line and ');' in line:
                line_split = line.split('(')
                func_decl = line_split[0].split()
                if len(func_decl) < 2: continue
                
                func_name = func_decl[-1].replace('*','')
                ret_type = func_decl[0]
                
                if not func_name or 'void' in func_name: continue

                if self.mode == OutputType.CPP:
                    target = f'{namespace}::{func_name}' if namespace else func_name
                    prefix = f'lua["{namespace}"]' if namespace else 'lua'
                    if namespace:
                         out.write(f'{prefix}["{func_name}"] = {target};')
                    else:
                         out.write(f'{prefix}.set_function("{func_name}", {target});')
                else:
                    arg_str = line_split[1].split(')')[0]
                    args = []
                    if arg_str and arg_str != 'void':
                        raw_args = arg_str.split(',')
                        for i, raw in enumerate(raw_args):
                            raw = raw.strip().replace('const ', '')
                            parts = raw.split()
                            a_name = f'arg{i}'
                            a_type = 'any'
                            if len(parts) >= 2:
                                a_name = parts[-1].replace('*', '')
                                a_type = parts[-2] if '*' not in parts[-1] else parts[-1]
                            elif len(parts) == 1:
                                a_type = parts[0]
                            args.append(f'{a_name}')
                            out.write(f'---@param {a_name} {self.sanitize_type(a_type)}')

                    out.write(f'---@return {self.sanitize_type(ret_type)}')
                    target_name = f'{namespace}.{func_name}' if namespace else func_name
                    out.write(f'function {target_name}({", ".join(args)}) end')

    def parse_events(self, file_path: Path, buffer_key='events'):
        lines = self.read_and_filter_lines(file_path)
        if not lines: return

        out = self.bufs[buffer_key]
        event_name = ""
        members = []
        in_event = False

        for line in lines:
            line = line.strip()
            if 'DEFINE_EVENT' in line:
                parts = line.split('(')[1].split(')')[0].split(',')
                event_name = parts[0].strip()
                self.event_list.append(event_name)
                members = []
                if ');' in line: 
                    self._write_event(event_name, [], out)
                else:
                    in_event = True
                continue

            if in_event:
                if ');' in line:
                    in_event = False
                    self._write_event(event_name, members, out)
                elif ';' in line:
                    m_name = line.split(';')[0].split()[-1].replace('*', '')
                    members.append(m_name)

    def _write_event(self, name, members, out):
        if self.mode == OutputType.CPP:
            out.write(f'lua.new_usertype<{name}>("{name}",')
            for i, m in enumerate(members):
                term = "," if i < len(members)-1 else ""
                out.write(f'    "{m}", sol::property(&{name}::{m}, &{name}::{m}){term}')
            out.write(');\n')
        else:
            out.write(f'---@class {name}')
            for m in members:
                out.write(f'---@field {m} any')
            out.write(f'{name} = {{}}\n')

    # --- Main Routine ---

    def run(self):
        if self.mode == OutputType.LUA:
            self.write_lua_header()

        # 1. Recursive Scan
        search_targets = [
            (Path("include"), "*"), 
            (Path("src"), "*.h")
        ]

        for search_path, pattern in search_targets:
            if search_path.exists():
                for file_path in search_path.rglob(pattern):
                    if not file_path.is_file() or self.is_blacklisted(file_path):
                        continue
                    
                    if "UIWidgets.h" in file_path.name: continue

                    is_scripting = 'scripting.h' in str(file_path)
                    self.parse_enums(file_path, as_value=is_scripting)
                    self.parse_structs(file_path)
                    self.parse_externs(file_path)

        # 2. Specific Files
        specific_files = [
            ("src/port/hooks/impl/EventSystem.h", ['enums', 'structs']),
            ("libultraship/src/public/bridge/consolevariablebridge.h", ['externs']),
            ("src/port/Engine.h", ['enums', 'externs']),
            ("libultraship/src/public/bridge/resourcebridge.h", ['externs']),
        ]

        for path_str, types in specific_files:
            p = Path(path_str)
            if 'enums' in types: self.parse_enums(p)
            if 'structs' in types: self.parse_structs(p)
            if 'externs' in types: self.parse_externs(p)

        # 3. Special Cases
        self.parse_externs(Path("src/port/ui/UIWidgets.h"), namespace='UIWidgets')

        # 4. Events Loop
        hooks_dir = Path("src/port/hooks/list")
        if hooks_dir.exists():
            for file_path in hooks_dir.rglob("*"):
                if file_path.is_file():
                    self.parse_enums(file_path) # Enums often in hooks too
                    self.parse_events(file_path)

        # 5. Finalize Output
        if self.mode == OutputType.CPP:
            self.save_cpp_output()
        else:
            print('---@enum EventID')
            print('EventID = {')
            for i, evt in enumerate(self.event_list):
                term = "," if i < len(self.event_list)-1 else ""
                print(f'    {evt} = -1{term}')
            print('}')

    def save_cpp_output(self):
        out_dir = Path('bindings/v1')
        out_dir.mkdir(parents=True, exist_ok=True)
        
        files = {
            'structs.gen': self.bufs['structs'],
            'externs.gen': self.bufs['externs'],
            'enums.gen': self.bufs['enums'],
            'events.gen': self.bufs['events'],
        }

        for fname, buf in files.items():
            with open(out_dir / fname, 'w', encoding='utf-8') as f:
                f.write('\n'.join(buf.get_content()))
        print(f"Generated C++ bindings in {out_dir}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Auto-generate Lua/C++ bindings")
    parser.add_argument("type", choices=['lua', 'cpp'], help="Output type")
    args = parser.parse_args()

    mode = OutputType.LUA if args.type == 'lua' else OutputType.CPP
    
    generator = BindingGenerator(mode)
    generator.run()