open Pyrel_lib

module SSet = Set.Make (String)

let read_all path =
  let ic = open_in path in
  Fun.protect
    (fun () -> really_input_string ic (in_channel_length ic))
    ~finally:(fun () -> close_in_noerr ic)

let canonical_path path =
  let path = if Filename.is_relative path then Filename.concat (Sys.getcwd ()) path else path in
  try Unix.realpath path with _ -> path

let quoted_path line =
  match String.index_opt line '"' with
  | None -> None
  | Some a ->
      (match String.index_from_opt line (a + 1) '"' with
       | None -> None
       | Some b -> Some (String.sub line (a + 1) (b - a - 1)))

let directive_path keyword line =
  let n = String.length keyword in
  if String.length line < n || String.sub line 0 n <> keyword then None
  else
    let rest = String.sub line n (String.length line - n) |> String.trim in
    match quoted_path rest with
    | None -> None
    | Some path ->
        let after =
          match String.index_opt rest '"' with
          | None -> ""
          | Some a ->
              (match String.index_from_opt rest (a + 1) '"' with
               | None -> ""
               | Some b -> String.sub rest (b + 1) (String.length rest - b - 1) |> String.trim)
        in
        if after = "" || after = ";" then Some path else None

let rec expand_file active loaded path =
  let path = canonical_path path in
  if SSet.mem path active then failwith ("cyclic include: " ^ path)
  else if SSet.mem path loaded then ("", "", loaded)
  else
    let active = SSet.add path active in
    let loaded = SSet.add path loaded in
    let dir = Filename.dirname path in
    let pyrel = Buffer.create 256 in
    let c = Buffer.create 256 in
    let lines = String.split_on_char '\n' (read_all path) in
    let loaded =
      List.fold_left
        (fun loaded line ->
          let t = String.trim line in
          match directive_path "includec" t with
          | Some p ->
              let cp = canonical_path (Filename.concat dir p) in
              if SSet.mem cp loaded then loaded
              else (Buffer.add_string c (read_all cp); Buffer.add_char c '\n'; SSet.add cp loaded)
          | None ->
              (match directive_path "include" t with
               | Some p ->
                   let a, cc, loaded = expand_file active loaded (Filename.concat dir p) in
                   Buffer.add_string pyrel a;
                   Buffer.add_string c cc;
                   loaded
               | None ->
                   Buffer.add_string pyrel line;
                   Buffer.add_char pyrel '\n';
                   loaded))
        loaded lines
    in
    Buffer.contents pyrel, Buffer.contents c, SSet.remove path active |> fun _ -> loaded

let () =
  if Array.length Sys.argv < 2 then begin
    print_endline "Usage: pyrel <filename>";
    exit 2
  end else
    let filename = Sys.argv.(1) in
    try
      let source, c_includes, _ = expand_file SSet.empty SSet.empty filename in
      let lexbuf = Lexing.from_string source in
      let program =
        try Parser.prog Lexer.read lexbuf
        with Parser.Error ->
          let p = Lexing.lexeme_start_p lexbuf in
          Printf.eprintf "Parser error at line %d, column %d\n" p.Lexing.pos_lnum (p.Lexing.pos_cnum - p.Lexing.pos_bol);
          raise Parser.Error
      in
      match Typechecker.check program with
      | Some err ->
          Printf.eprintf "Type Error: %s\n" err;
          exit 1
      | None ->
          let feature_prelude = "#if !defined(_WIN32)\n#define _POSIX_C_SOURCE 200809L\n#define _XOPEN_SOURCE 700\n#endif\n" in
          let c_code = feature_prelude ^ c_includes ^ "\n" ^ Compiler.compile program in
          let out_c = filename ^ ".c" in
          let oc = open_out out_c in
          Fun.protect (fun () -> output_string oc c_code) ~finally:(fun () -> close_out_noerr oc);
          let out_exe = Filename.chop_extension filename in
          let cmd = Printf.sprintf "gcc -std=c11 %s -o %s" out_c out_exe in
          let exit_code = Sys.command cmd in
          if exit_code = 0 then Printf.printf "Successfully compiled to %s\n" out_exe
          else begin
            Printf.eprintf "Error: gcc failed with exit code %d\n" exit_code;
            exit 1
          end
    with
    | Parser.Error ->
        print_endline "Syntax error in Pyrel source";
        exit 1
    | Sys_error e ->
        Printf.eprintf "I/O error: %s\n" e;
        exit 1
    | Failure e ->
        Printf.eprintf "Include error: %s\n" e;
        exit 1
    | e ->
        Printf.eprintf "%s\n" (Printexc.to_string e);
        exit 1
