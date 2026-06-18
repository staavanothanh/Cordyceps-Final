Dưới đây là bản tổng hợp toàn diện (Master Blueprint) từ cuộc trò chuyện của chúng ta để bạn và đồng đội làm tài liệu hướng dẫn phát triển (Development Guide):

---

## I. THÔNG TIN CUỘC THI & TRIẾT LÝ CHIẾN THUẬT

### 1. Thể thức thi đấu (Master Track)

* **Format:** Đối đầu trực tiếp (AI-vs-AI Head-to-Head Battle). Bạn nộp file mã nguồn C++ cùng file `data.bin`, hệ thống Arena của Nexon sẽ tự động cho Agent của bạn đấu với đội khác.
* **Môi trường Server:** Chạy trên **AWS c7a.2xlarge** (Ubuntu 24.04, giới hạn nghiêm ngặt **1 CPU Core**, tài nguyên RAM tối đa **1,024 MiB**). Không có kết nối mạng khi chấm thi.
* **Thể thức vòng loại (Swiss Tournament):** Hệ thống tính điểm dựa trên kết quả trận đấu (Thắng = 1đ, Hòa = 0.5đ, Thua = 0đ). Bạn sẽ liên tục bị xếp cặp với các đối thủ có cùng phong độ (càng về sau đối thủ càng mạnh).

### 2. Triết lý thiết kế Agent ("The Chimera")

Để chiến thắng một giải đấu dài hơi, Agent không được dùng một thuật toán duy nhất mà phải là một thực thể lai hợp kết hợp nhiều mô-đun (gọi là kiến trúc **Chimera**):

* **Khai cuộc (Early Game):** Ưu tiên tốc độ, sử dụng bảng tra cứu tính sẵn trong `data.bin` để đi quân dưới 1ms, bảo toàn quỹ thời gian.
* **Trung cuộc (Midgame):** Kích hoạt **MCTS (Monte Carlo Tree Search)** để tính toán thời gian thực nhằm đối phó với các thế cờ biến hóa nằm ngoài dữ liệu tính sẵn. Đồng thời sử dụng mô-đun **Opponent Modeling** để nhận diện và khắc chế phong cách của đối thủ (Aggressive/Defensive).
* **Tàn cuộc (Endgame):** Khi bàn cờ còn ít nấm, kích hoạt **Minimax Alpha-Beta** để "vét cạn" toàn bộ nhánh cờ, đưa ra nước đi chính xác tuyệt đối nhằm dứt điểm ván đấu.

---

## II. ĐẶC TẢ CHI TIẾT VỀ "MUSHROOM GAME" (TRÒ CHƠI HÁI NẤM)

### 1. Luật chơi cốt lõi

* **Bàn cờ:** Một lưới ô vuông kích thước cố định **10 hàng × 17 cột** (tổng cộng 170 ô). Mỗi ô ban đầu chứa một cây nấm có giá trị từ $1$ đến $9$.
* **Cách đi quân:** Chọn một vùng hình chữ nhật $(r_1, c_1, r_2, c_2)$ sao cho **Tổng giá trị của tất cả các cây nấm còn sót lại nằm TRONG và TRÊN BIÊN của hình chữ nhật đó phải bằng đúng $10$**. Các ô trống (nấm đã bị đào trước đó) tính là $0$.
* **Cơ chế chiếm đất:** Khi chọn vùng hợp lệ, nấm ở đó bị xóa. Bạn sẽ làm chủ toàn bộ ô đất trong hình chữ nhật đó. Nếu vùng đó chứa ô đất vốn thuộc về đối thủ, bạn **cướp luôn quyền sở hữu** các ô đó.
* **Luật Tiếp Xúc Biên (Inscribed Rule - Chí mạng):** Cả 4 cạnh (rìa Trên, Dưới, Trái, Phải) của hình chữ nhật bạn chọn **bắt buộc phải chạm vào ít nhất một cây nấm còn sống**. Bạn không được phép vẽ biên hình chữ nhật nằm hoàn toàn trên các ô trống.
* **Kết thúc:** Người chơi có thể chọn Bỏ lượt (Pass) bằng cách xuất ra `-1 -1 -1 -1`. Game kết thúc khi **cả hai bên cùng Pass liên tiếp**. Ai chiếm được nhiều ô đất hơn ở cuối trận sẽ Thắng.

### 2. Giao thức dòng lệnh (Standard I/O Protocol)

Agent của bạn phải đọc dữ liệu từ `std::cin` và in kết quả ra `std::cout`, kết thúc bằng lệnh xóa bộ đệm (`std::endl` hoặc `std::flush`).

* `READY (FIRST|SECOND)`: Hệ thống báo lượt đi trước/sau. C++ phải phản hồi `OK` trong vòng 3 giây.
* `INIT ...`: Hệ thống gửi 10 chuỗi (mỗi chuỗi 17 ký số) đại diện cho bàn cờ ban đầu.
* `TIME t1 t2`: Đến lượt bạn! `t1` là thời gian còn lại của bạn (Tổng thời gian cả trận là 10,000ms). Bạn phải in ra tọa độ `r1 c1 r2 c2` hoặc `-1 -1 -1 -1`.
* `OPP r1 c1 r2 c2 t`: Nhận thông tin nước đi vừa rồi của đối thủ để cập nhật lại bàn cờ nội bộ.
* `FINISH`: Game kết thúc, chương trình C++ phải lập tức thoát (`return 0;`).

---

## III. KIẾN TRÚC XÂY DỰNG ENGINE C++ TRUYỀN THỐNG

Về bản chất, bạn đang xây dựng một Engine cờ tương tự như cấu trúc nền tảng của **Stockfish**. Kiến trúc bao gồm 3 thành phần chính:

### 1. Move Generator (Bộ sinh nước đi hợp lệ)

* Nhiệm vụ: Quét ma trận nấm hiện tại và trả về danh sách tất cả các hình chữ nhật thỏa mãn luật Tổng = 10 và Luật Tiếp xúc biên.
* **Tối ưu bằng Prefix Sum 2D:** Hãy xây dựng một mảng cộng dồn 2 chiều (Prefix Sum Matrix). Nhờ mảng này, việc tính tổng của bất kỳ hình chữ nhật nào trên bàn cờ sẽ đạt tốc độ **$\mathcal{O}(1)$ tuyệt đối**, thay vì phải dùng vòng lặp cộng từng ô.

### 2. Evaluation Function (Hàm Heuristic đánh giá thế cờ)

Khi cây tìm kiếm không thể đi hết đến cuối ván, hàm này sẽ chấm điểm trạng thái bàn cờ giả lập để AI quyết định:


$$\text{Score} = w_1 \times (\text{Số ô đất của mình}) - w_2 \times (\text{Số ô đất của địch}) + w_3 \times (\text{Chỉ số ép đối thủ Pass})$$


*Bộ trọng số ($w_1, w_2, w_3$) này sẽ được bạn tối ưu hóa thông qua hàng ngàn ván tự đấu (Self-play) ở nhà trước khi nộp bài.*

### 3. Search Engine (Bộ tìm kiếm sâu)

* **MCTS (Trung cuộc):** Giả lập hàng triệu trận đấu ngẫu nhiên từ thế cờ hiện tại để tìm ra nước đi có xác suất thắng cao nhất.
* **Minimax Alpha-Beta (Tàn cuộc):** Thực hiện duyệt vét cạn tất cả các nước đi khi số lượng nấm trên bàn cờ còn ít (nhánh tìm kiếm hẹp) để tìm ra chuỗi nước đi chiếu bí đối phương.

---

## IV. CÁC LƯU Ý KỸ THUẬT VÀ BẪY BẢO MẬT BỘ NHỚ (CRITICAL)

Để tránh tình trạng "sập nguồn phần mềm" (Crash chương trình dẫn đến bị xử thua `ABORT` trên server), bạn phải quản lý C++ ở mức an toàn tuyệt đối:

1. **Né bẫy Segmentation Fault:** Không sử dụng mảng thô kiểu C (`int board[10][17]`). Hãy thay thế bằng `std::array<std::array<int, 17>, 10>`. Khi truy cập phần tử để tính toán, hãy dùng hàm `.at(r).at(c)` trong giai đoạn debug để trình biên dịch tự động bắt lỗi nếu thuật toán tính toán sai làm vượt quá biên chỉ số mảng.
2. **Chặn đứng rò rỉ bộ nhớ (Memory Leak):** Cây tìm kiếm MCTS sẽ sinh ra hàng triệu Node bộ nhớ mỗi lượt. Hãy sử dụng con trỏ thông minh `std::unique_ptr` để quản lý các Node con. Sau khi kết thúc lệnh `TIME`, bắt buộc phải giải phóng toàn bộ cây cũ để giữ lượng RAM tiêu thụ luôn dưới mức **1,024 MiB**.
3. **Hạ chuẩn biên dịch xuống C++20:** Trình biên dịch của BTC là **GCC 14.2.0** (hỗ trợ C++20/C++23). Ở máy nhà bạn dùng GCC 15, do đó **không được viết các cú pháp mới của C++26**. Hãy ép trình biên dịch ở nhà chạy chuẩn bằng cờ: `-std=c++20 -O3`. (Cờ `-O3` là bắt buộc để kích hoạt tối ưu hóa mã máy tối đa).
4. **Thiết lập Safeguard Timer (Bộ đếm giờ phòng vệ):** Tổng thời gian trận đấu chỉ có 10 giây. Trong vòng lặp MCTS, cứ sau 1000 lượt duyệt, phải kiểm tra thời gian thực tế. Nếu lượt đi đã ngốn quá thời gian budget định sẵn (ví dụ: 150ms), phải lập tức ngắt vòng lặp và `return` ngay nước đi tốt nhất đang có, tuyệt đối không được cố tính toán thêm dẫn đến dính lỗi **Time Limit Exceeded (TLE)**.
5. **Vũ khí Bitmask:** Vì bàn cờ chỉ có 170 ô, hãy dùng các số nguyên `uint64_t` để làm Bitmask lưu trữ quyền sở hữu đất. Việc copy thế cờ để giả lập trong MCTS lúc này chỉ là các phép toán logic bit trên thanh ghi CPU, giúp tăng tốc độ duyệt cây lên hàng triệu Nodes/giây.

---

## V. CÁC CÔNG CỤ CẦN THIẾT TRÊN MÁY TÍNH CỦA BẠN

Để chuẩn bị môi trường phát triển chuyên nghiệp, đội của bạn cần cài đặt các công cụ sau:

* **WSL2 (Ubuntu 24.04 LTS):** Cài đặt phân vùng Linux này trên Windows để mô phỏng chính xác môi trường server AWS của ban tổ chức. Hãy cài đặt `g++-14` trong WSL2 để đồng bộ trình biên dịch với BTC.
* **File `data.bin` (Tối đa 10MB):** Viết một chương trình C++ phụ chạy ở máy nhà, cho nó chạy liên tục nhiều giờ liền để tính toán trước các thế cờ khai cuộc/tàn cuộc tối ưu, sau đó dùng `std::ofstream` xuất ra file nhị phân `data.bin`. Khi nộp bài, file này sẽ được nạp thẳng vào RAM khi nhận lệnh `READY`.
* **Bộ công cụ của BTC:** Sử dụng file `testing_tool.py` (hoặc bản GUI) mà bạn đã tải từ trang [NYPC Problems](https://contest.nypc.co.kr/problems/1) về để làm trọng tài, cấu hình file `setting.ini` trỏ vào file thực thi `.exe` hoặc file chạy Linux của Bot C++ để tiến hành test đối kháng nội bộ (Internal Tournament) ít nhất 100 trận sau mỗi lần thay đổi thuật toán.

---
