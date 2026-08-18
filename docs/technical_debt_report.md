# Báo cáo technical debt của Pyrel

**Ngày đánh giá:** 18/08/2026
**Revision được đánh giá:** `46f601b` trên `origin/main`
**Phạm vi:** Host compiler, Bootstrap compiler, standard library, test runners và các regression/stress fixtures.

## 1. Tóm tắt điều hành

Đợt dọn dẹp gần nhất đã xử lý phần technical debt rõ ràng nhất trong standard library: các implementation container chuyên biệt cho `int`, kiểu `Map<T>` pair không còn phù hợp với thiết kế hiện tại, các alias Result/Option trùng lặp và một số nhánh growth không thể đạt tới. Các fixture còn sử dụng API cũ đã được chuyển sang `Slice<int>` và `HashMap<int, int>`, đồng thời được đưa vào regression runner để tránh tái phát. Commit này loại bỏ **235 dòng** và bổ sung **49 dòng** test/runner code [1].

Nền tảng container hiện đã thống nhất theo hướng generic và dogfooding bằng Pyrel. Commit trước đó đã hoàn thiện auto-growth cho `Array<T>`, `Slice<T>` và `HashMap<K,V>`, bổ sung regression cho nhiều primitive type, đồng thời sửa lỗi Bootstrap suy luận witness type của generic local/field để Host và Bootstrap sinh C tương thích [2].

Tuy nhiên, technical debt chưa biến mất hoàn toàn. Ba vùng cần ưu tiên tiếp theo là **Bootstrap compiler monolithic**, **parity giữa Host typechecker và Bootstrap typechecker**, và **thiết kế thực sự của HashMap**. Ngoài ra, string hiện vẫn là abstraction UTF-8 chưa hoàn chỉnh, còn Array/Slice có API chồng lấn và ownership của runtime vẫn phụ thuộc nhiều vào cơ chế tracking toàn cục.

## 2. Technical debt đã xử lý

| Hạng mục | Trạng thái xử lý | Tác động |
|---|---|---|
| Built-in dynamic array legacy như `array_make`, `array_push` | Đã loại bỏ khỏi Host và Bootstrap | Container không còn bị chia đôi giữa compiler runtime và stdlib Pyrel [3]. |
| Wrapper memory stdlib dư thừa | Đã loại bỏ | Giảm một tầng API chỉ chuyển tiếp tới `memory_alloc`, `memory_resize`, `memory_free` [4]. |
| Array implementation không thuần Pyrel | Đã chuyển sang `src/stdlib/array.pyrel` | Growth policy, indexing và higher-order operations nằm trong stdlib thay vì OCaml [5]. |
| Container chỉ hỗ trợ `int` | Đã thay bằng generic `Array<T>`, `Slice<T>`, `HashMap<K,V>` | Hỗ trợ typed allocation và growth cho các kiểu Pyrel được typechecker chấp nhận [2]. |
| Bootstrap mất binding K/V ở witness local | Đã sửa | Codegen `memory_alloc` hiện có thể giữ đúng kiểu substituted generic, tránh sinh nhầm `(int *)` cho `double*` hoặc `char*` [2]. |
| `IntSlice` và toàn bộ `*_int` API | Đã xóa | Các test đã chuyển sang `Slice<int>` generic; không còn implementation trùng lặp [1]. |
| `IntMap` và toàn bộ `*_int` API | Đã xóa | Các test đã chuyển sang `HashMap<int,int>` generic; giảm hai code path probing/growth [1]. |
| `Map<T>` pair legacy | Đã xóa | Probe cũ được viết lại để kiểm tra `remove`, `clear`, `length` của HashMap thực tế [1]. |
| Alias `Result::value_or`, `Option::value_or_option` | Đã xóa | Giảm public API trùng với `unwrap_or` và `unwrap_or_option` [1]. |
| Nhánh `grown == next` trong growth loop | Đã xóa | Đây là nhánh không thể xảy ra với capacity dương; logic overflow vẫn giữ qua điều kiện `grown < next` [1]. |
| Regression coverage cho container | Đã mở rộng | Runner kiểm tra growth, container generic, stress loop và generic map probe qua cả Host và Bootstrap [1] [2]. |

Một điểm quan trọng là các allocator `alloc_ints`, `grow_ints` và `free_ints` **không bị xóa tùy tiện**. Chúng vẫn được dùng bởi Bootstrap compiler và một số pointer/ownership fixtures; xóa chúng khỏi runtime hiện tại sẽ phá các call site hợp lệ. Đây là ví dụ về phần code có tên cũ nhưng chưa phải dead code.

## 3. Kết quả kiểm chứng hiện tại

Full validation gần nhất đã chạy thành công sau đợt cleanup. Pipeline bao gồm ownership fixtures, regression, stress, adversarial, conformance và fixed-point production [6].

| Kiểm chứng | Kết quả |
|---|---|
| Host compiler build | PASS |
| Bootstrap compiler build | PASS |
| Strict GCC với `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` | PASS |
| Host/Bootstrap runtime parity | PASS |
| Ownership valid/invalid fixtures | PASS |
| Regression suite mở rộng | PASS |
| Stress suite | PASS |
| Adversarial suite | PASS |
| Conformance suite | PASS |
| Fixed-point `n2.c == n3.c` | PASS |
| Legacy container reference scan | Không còn `IntMap`, `IntSlice`, `Map<T>` hoặc các API đã xóa |
| Repository hygiene | Không còn ELF ngoài `.tmp`; working tree sạch sau push |

## 4. Module cần chú ý tiếp theo

### 4.1. Bootstrap compiler: `src/bootstrap/pyrelc.pyrel` — mức độ **Rất cao**

Bootstrap compiler hiện là một file lớn, chứa lexer/parser, AST payload, symbol table, typechecker, generic specialization và C emitter. Bản thân source Bootstrap khoảng **3.745 dòng**, còn canonical C sinh ra khoảng **10.934 dòng**. Việc duy trì cả Pyrel source và C generated artifact khiến mọi thay đổi compiler đều có chi phí parity và fixed-point cao [7].

Lỗi witness `K/V` vừa xử lý cho thấy type information hiện vẫn đi qua nhiều dạng biểu diễn song song: summary primitive kind, full type node, generic binding và `node_aux`. Đây là nguồn rủi ro tái diễn, dù fixed-point hiện đang pass.

**Khuyến nghị:** tách Bootstrap source thành các module logic rõ ràng hoặc ít nhất phân vùng nội bộ theo lexer, parser, typechecker, generic collection và emitter; sau đó chuẩn hóa một representation cho type thay vì để codegen tự fallback giữa nhiều bảng metadata. Mỗi thay đổi type system nên có một differential test Host/Bootstrap riêng.

### 4.2. Host/Bootstrap typechecker parity — mức độ **Cao**

Host typechecker nằm trong `src/compiler/lib/typechecker.ml`, trong khi Bootstrap có một bản port Pyrel tương ứng. Hai implementation này vẫn phải đồng bộ thủ công. Sự khác biệt giữa `sym_type`, `tc_var_type`, `node_aux` và substituted generic type từng trực tiếp gây ra code C sai kiểu trong `map::reserve<K,V>` [2] [8].

**Khuyến nghị:** xây dựng một nhóm parity tests theo từng lớp type: primitive, pointer, named type, generic parameter, generic field access, nested generic và function pointer. Diagnostic message cũng nên được chuẩn hóa để cùng một lỗi có cấu trúc và vị trí tương đương ở Host/Bootstrap.

### 4.3. `src/stdlib/map.pyrel` — mức độ **Cao**

Tên module là HashMap, nhưng implementation hiện tại chưa thực hiện hashing theo key. `find` bắt đầu từ slot 0 và quét tuần tự; `insert_raw` cũng tìm slot trống theo thứ tự tuyến tính. Vì vậy, đây là **linear-probing table với full scan**, chưa phải hash map theo nghĩa hiệu năng thông thường [9]. Tombstone state đã tồn tại, nhưng chưa có compaction riêng và hiệu năng xấu nhất vẫn là O(capacity) cho lookup/insertion.

Generic key hiện phụ thuộc vào khả năng so sánh `K == K`, trong khi hash function cho mọi Pyrel value type chưa được thiết kế. Đây là ràng buộc kiến trúc, không nên giải quyết bằng cách nhét thêm các nhánh primitive vào `map.pyrel`.

**Khuyến nghị:** chọn một trong hai hướng rõ ràng. Hoặc đổi tên/đặc tả thành flat map để phản ánh đúng semantics hiện tại; hoặc thiết kế trait/primitive contract cho `hash(K)` và `eq(K,K)`, sau đó dùng hash seed để chọn vị trí ban đầu và rehash có kiểm soát. Cần bổ sung benchmark collision, tombstone-heavy workload và named/string key tests.

### 4.4. `src/stdlib/array.pyrel` và `src/stdlib/slice.pyrel` — mức độ **Trung bình đến cao**

Hai module đã generic và cùng hỗ trợ growth, nhưng API vẫn có phần chồng lấn. `Array<T>` có `slice`, `map`, `filter`; `Slice<T>` là owning growable container nhưng có bộ thao tác khác và quy ước return value chưa hoàn toàn thống nhất. Ví dụ, một số mutator trả lại container để hỗ trợ assignment, trong khi một số hàm chỉ mutate backing storage. Điều này làm tăng chi phí học API và tạo thêm bề mặt cần test [10] [11].

**Khuyến nghị:** viết một API contract ngắn cho container: quy ước ownership, ý nghĩa của `free`, behavior khi index ngoài biên, growth guarantee, và hàm nào mutate tại chỗ. Sau đó quyết định rõ `Array<T>` là fixed-capacity owning buffer có growth tùy chọn hay là dynamic vector; `Slice<T>` nên là view hay owning container. Hiện `Slice<T>` đang là owning, nên tên/đặc tả cần phản ánh điều đó.

Coverage cũng nên mở rộng từ `int`, `bool`, `char`, `double` sang `float`, `string` và named/struct type. Đây là các trường hợp dễ làm lộ lỗi witness type, copy semantics và zero-value initialization.

### 4.5. `src/stdlib/string.pyrel` — mức độ **Cao**

Module đã có UTF-8 helpers, nhưng abstraction cơ bản vẫn byte-oriented: `byte_len` đếm byte và `at` trả byte trong miền 0..255. Các API code point như `codepoint_len` và `codepoint_at` tồn tại song song, nên nếu không có quy ước rõ ràng người dùng dễ nhầm byte index với code point index [12].

`concat` vẫn là phép nối tạo allocation mới. Khi dùng trong vòng lặp hoặc stress workload, ownership và chi phí allocation cần được mô tả rõ; nếu không, người dùng khó biết khi nào phải giữ, chuyển hoặc giải phóng chuỗi.

**Khuyến nghị:** tách rõ nhóm `byte_*` và `codepoint_*`, bổ sung test malformed UTF-8, truncated sequence, boundary code point và indexing ngoài biên. Về dài hạn nên có string view/borrowed slice để giảm allocation trung gian, nhưng chỉ triển khai sau khi ownership contract được chốt.

### 4.6. Ownership/runtime memory tracking — mức độ **Cao**

Các container hiện gọi trực tiếp `memory_alloc`, `memory_resize` và `memory_free`, còn runtime vẫn dựa nhiều vào registry tracking và cleanup toàn cục. Ownership stress và sanitizer hiện pass, nhưng cơ chế này chưa thay thế cho ownership model rõ ràng ở cấp ngôn ngữ. Các API trả struct có pointer như `Array<T>`, `Slice<T>` và `HashMap<K,V>` vẫn cần quy ước chặt về copy, move, alias và double-free.

**Khuyến nghị:** lập bảng ownership contract cho từng hàm public; bổ sung negative tests cho copy-after-free, use-after-move, resize qua alias và free lặp lại. Chỉ sau khi contract ổn định mới xem xét tối ưu registry hoặc chuyển sang borrow checker mạnh hơn.

### 4.7. Test orchestration và artifact hygiene — mức độ **Trung bình**

Full suite hiện có chất lượng tốt và fixed-point pass, nhưng một số fixture tồn tại ngoài runner chính hoặc được gọi qua các script khác nhau. Các runner cũng tạo nhiều C file và executable tạm; dù đã dọn thủ công và repository hiện sạch, đây vẫn là nguồn noise và nguy cơ accidentally commit artifact.

**Khuyến nghị:** dùng một manifest test duy nhất hoặc helper chung cho Host/Bootstrap; đặt mọi output vào `.tmp/<suite>`; thêm `trap` cleanup; và có một CI check fail nếu repository chứa ELF hoặc generated C ngoài thư mục được phép. Đồng thời ghi rõ fixture nào là compile-only, runtime parity, sanitizer hay rejection test.

## 5. Thứ tự ưu tiên đề xuất

| Ưu tiên | Công việc | Lý do |
|---|---|---|
| P0 | Chuẩn hóa type representation và Host/Bootstrap parity | Đây là nguồn rủi ro có thể sinh C sai kiểu dù test cơ bản vẫn pass. |
| P0 | Chốt semantics và ownership contract của container | Array/Slice/HashMap đều giữ raw pointer; ambiguity sẽ tạo lỗi memory khó truy vết. |
| P1 | Thiết kế hashing/equality cho `HashMap<K,V>` | Implementation hiện đúng chức năng cơ bản nhưng chưa có hiệu năng của hash map thật. |
| P1 | Chuẩn hóa byte/code point API của string | Tránh lỗi Unicode và giảm allocation không cần thiết. |
| P1 | Tách hoặc cấu trúc lại Bootstrap compiler | Giảm chi phí fixed-point và nguy cơ drift giữa các subsystem. |
| P2 | Hợp nhất test manifest và cleanup artifacts | Tăng độ tin cậy dài hạn, giảm chi phí vận hành suite. |
| P2 | Mở rộng generic tests cho float, string, named/struct values | Bao phủ các vùng dễ phát sinh lỗi type substitution và ownership. |

## 6. Kết luận

Đợt cleanup đã xử lý đúng nhóm debt gây nhiễu và trùng lặp nhất: các API container chuyên biệt cho `int` và các compatibility implementation không còn cần thiết. Thiết kế hiện tại đã tiến gần hơn tới một standard library generic, thuần Pyrel và có regression protection rõ ràng.

Phần cần tránh tiếp theo không phải là thêm nhiều API mới, mà là **giảm số lượng representation và code path song song**. Cụ thể, nên ưu tiên type representation dùng chung giữa Host/Bootstrap, ownership contract cho container, rồi mới nâng cấp HashMap và string. Nếu thực hiện theo thứ tự này, các thay đổi sau sẽ ít tạo technical debt mới hơn và fixed-point sẽ dễ duy trì hơn.

## References

[1]: https://github.com/memeviber/Pyrel/commit/46f601b "Remove obsolete container compatibility code"

[2]: https://github.com/memeviber/Pyrel/commit/7ec8baa "Add generic auto-growing containers and fix Bootstrap witnesses"

[3]: https://github.com/memeviber/Pyrel/commit/bab8c31 "Remove legacy array built-ins and add unary minus"

[4]: https://github.com/memeviber/Pyrel/commit/c262a7d "Remove redundant memory stdlib wrapper"

[5]: https://github.com/memeviber/Pyrel/commit/d03c592 "Rewrite array stdlib as pure Pyrel with memory boundary"

[6]: https://github.com/memeviber/Pyrel/blob/main/scripts/run_ownership_stress.sh "Full ownership and validation runner"

[7]: https://github.com/memeviber/Pyrel/blob/main/src/bootstrap/pyrelc.pyrel "Pyrel Bootstrap compiler source"

[8]: https://github.com/memeviber/Pyrel/blob/main/src/compiler/lib/typechecker.ml "Host typechecker"

[9]: https://github.com/memeviber/Pyrel/blob/main/src/stdlib/map.pyrel "Generic HashMap standard-library module"

[10]: https://github.com/memeviber/Pyrel/blob/main/src/stdlib/array.pyrel "Generic Array standard-library module"

[11]: https://github.com/memeviber/Pyrel/blob/main/src/stdlib/slice.pyrel "Generic Slice standard-library module"

[12]: https://github.com/memeviber/Pyrel/blob/main/src/stdlib/string.pyrel "String and UTF-8 standard-library module"

## 7. Addendum sau đợt hardening tiếp theo

Đợt hardening tiếp theo đã xử lý thêm các điểm có nguy cơ gây hậu quả về sau. Bootstrap hiện hỗ trợ indirect function calls với các dạng callee là biến, field access và grouped expression; các probe Host/Bootstrap tương ứng đã được kiểm tra. Pointer subtraction được thống nhất trả `int` ở type layer, emitter sinh cast C tường minh và format `print` dùng `%d`, loại bỏ cảnh báo `%td` không khớp dưới strict GCC.

`HashMap<K,V>` hiện có hai chế độ rõ ràng: chế độ fallback tuyến tính để giữ API cơ bản và chế độ hashed tùy chọn qua `with_hasher`/`new_with_hasher`, nhận callback `hash(K): int` và `equals(K,K): bool`. Rehash đã được sửa để tái chèn theo callback thay vì sao chép tuần tự; regression mới bao phủ collision, tombstone, clear, rehash và giá trị `double`.

Ownership của `Array<T>`, `Slice<T>` và `HashMap<K,V>` đã được làm rõ hơn bằng consuming-style `free`: caller phải gán lại giá trị trả về, ví dụ `m = map::free(m)`. Container sau free được reset pointer và metadata, có guard null, giúp giảm nguy cơ double-free và use-after-free do tiếp tục dùng bản struct cũ. API string mơ hồ `str::at` cũng đã được đổi thành `str::byte_at` để phân biệt rõ byte indexing với các helper code point.

Sau các thay đổi này, full validation vẫn đạt: ownership, regression, stress, adversarial, conformance, strict GCC và fixed-point; `n2.c` và `n3.c` vẫn byte-identical. Checksum production đã được cập nhật theo canonical `n2.c` mới.

Các rủi ro còn lại chủ yếu là **chuẩn hóa type representation dùng chung giữa Host/Bootstrap**, **mở rộng coverage cho string/named values và callback edge cases**, và **hợp nhất cleanup artifact thành cơ chế tự động trong runner**. Đây là các công việc tiếp theo, không phải lý do để trì hoãn việc sử dụng API hiện tại trong phạm vi đã được regression bảo vệ.

## 8. Addendum: C prologue và include bị phát sinh lặp

Đã loại bỏ duplication trong C output của Host compiler. Trước đây `main.ml` phát sinh riêng feature prelude cho `_POSIX_C_SOURCE` và `_XOPEN_SOURCE`, trong khi `compiler.ml` lại phát sinh cùng nhóm macro bên trong runtime prologue. `compiler.ml` đồng thời nối thêm lần thứ hai các include `<stdio.h>`, `<stdlib.h>` và `<string.h>`. Vì vậy output C có hai lớp logic môi trường và hai bộ include giống nhau.

Hiện `Compiler.compile` nhận raw `includec` payload qua tham số `c_includes` và đặt payload này sau **một runtime prologue trung tâm**. `main.ml` không còn tự nối feature prelude hoặc ghép include lần nữa. Cách sắp xếp này vừa loại bỏ code lặp, vừa bảo đảm macro môi trường xuất hiện trước các header C do `includec` cung cấp.

Regression `include_test_main` nay kiểm tra riêng cả Host và Bootstrap: mỗi macro/include chuẩn phải xuất hiện đúng một lần trong phần prologue thực tế, đồng thời phải vắng feature prelude cũ `#if !defined(_WIN32)`. Regression, strict GCC và fixed-point `n2.c == n3.c` đều đạt sau thay đổi.

| Hạng mục | Trước | Sau |
|---|---:|---:|
| Feature prelude `_POSIX_C_SOURCE`/`_XOPEN_SOURCE` trong output prologue | 2 | 1 |
| `<stdio.h>` trong output prologue | 2 | 1 |
| `<stdlib.h>` trong output prologue | 2 | 1 |
| `<string.h>` trong output prologue | 2 | 1 |
| Điểm phát sinh prologue | `main.ml` và `compiler.ml` | `compiler.ml` |

Đây là một structural cleanup, không thay đổi semantics của `include`, `includec`, runtime allocator hoặc generated program body.

## 9. References

[13]: https://github.com/memeviber/Pyrel/blob/main/src/compiler/lib/compiler.ml "Host C emitter and centralized runtime prologue"

[14]: https://github.com/memeviber/Pyrel/blob/main/scripts/run_regression.sh "Regression checks for Host/Bootstrap C prologue parity"
