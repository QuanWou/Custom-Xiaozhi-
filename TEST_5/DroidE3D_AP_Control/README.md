# Droid E3D v4 — cảm xúc phối hợp đầu, tay và xích

## Cài đặt

1. Giải nén, giữ `DroidE3D_AP_Control.ino` và `RobotUI.h` trong cùng thư mục `DroidE3D_AP_Control`.
2. Mở `.ino` trong Arduino IDE. Giữ cấu hình ESP32-S3 đã chạy được bản v2 / các bài test của bạn.
3. Cài thư viện **WebSockets by Markus Sattler (Links2004)**. WiFi, WebServer, Preferences và LEDC có sẵn trong gói ESP32. Không cần ESP32Servo.
4. Nạp lại chương trình, kết nối Wi-Fi **Droid-E3D**, mật khẩu **droidrobot123**.
5. Mở **http://192.168.4.1** và tải lại trang. Nhãn cạnh tên robot phải hiện **v4**.

Nên xoay ngang điện thoại. Giao diện cũng có bố cục hai cột cho màn hình dọc; danh sách động tác dài hơn và có thể cuộn.

## Giao diện chính

- Bên trái chỉ có các nút **Tiến, Lùi, Trái, Phải, DỪNG**. Giữ nút hướng để chạy, thả để dừng xích.
- Nút **DỪNG** màu hồng dừng cả hai xích và hủy động tác. Các khớp giữ ở bước góc phần mềm đang điều khiển.
- Bên phải là các nút động tác. Chạm một lần để thực hiện; nút đang chạy được viền màu. Chạm động tác khác để thay thế động tác hiện tại.
- **Dừng động tác** hủy chuỗi đang chạy, dừng cả xích do chuỗi điều khiển và giữ các khớp ở bước góc hiện tại.
- Nút **Hiệu chỉnh** mở popup, gom toàn bộ chỉnh góc, tốc độ, xung dừng và đảo chiều vào một chỗ. Mở hoặc đóng popup đều dừng robot.

## Các động tác

| Nút | Hoạt động |
|---|---|
| Nhìn trái | Đầu tới góc mặc định trừ 30°, giữ tại đó |
| Nhìn phải | Đầu tới góc mặc định cộng 30°, giữ tại đó |
| Ngó nghiêng | Nhìn hai phía rồi trở về góc mặc định |
| Vẫy tay trái | Vẫy riêng tay trái rồi về góc mặc định |
| Vẫy tay phải | Vẫy riêng tay phải rồi về góc mặc định |
| Vui mừng | Nâng/vẫy hai tay, quay đầu qua lại; xích tiến nhẹ, xoay thân luân phiên rồi lùi nhẹ |
| Buồn | Tay chuyển chậm về tư thế thấp, đầu nhìn lệch hai phía, thân lùi nhẹ một nhịp |
| Tò mò | Đầu nhìn hai phía cùng tay và thân xoay nhẹ, sau đó tiến/lùi ngắn |
| Chào bạn | Tiến nhẹ, vẫy hai tay luân phiên cùng đầu, sau đó lùi nhẹ |
| Lắc đầu | Lắc quanh góc mặc định ±25°, sau đó về giữa |
| Biểu diễn | Phối hợp cả 5 servo: tay luân phiên, đầu quay theo nhịp, thân xoay hai phía và tiến/lùi; kết thúc ở tư thế mặc định |
| Về tư thế chuẩn | Đưa đầu và hai tay về các góc mặc định đã lưu |

Động tác chạy bằng bộ định thời không chặn trong `.ino`; JavaScript chỉ gửi tên động tác và hiển thị trạng thái. Các nút Vui mừng, Buồn, Tò mò, Chào bạn và Biểu diễn điều khiển cả 5 servo. Các nút nhìn/vẫy riêng chỉ điều khiển khớp tương ứng và dừng xích khi bắt đầu. Chưa phát âm thanh. Đầu là servo 180° nên không có lệnh quay đầu liên tục 360°.

Nếu chiều cơ khí của đầu trái/phải bị ngược, đổi dấu độ lệch trong các bảng `LOOK_LEFT`, `LOOK_RIGHT`, `LOOK_AROUND` trong `.ino`. Các góc luôn được giới hạn 0–180°.

## Tư thế mặc định

- Đầu: **90°** — theo lần lắp lại mới nhất.
- Tay trái: **90°**.
- Tay phải: **90°**.

Trong popup, kéo góc một servo rồi bấm **Lưu làm mặc định** ở đúng servo đó. Giá trị được lưu vào bộ nhớ, áp dụng cho lần bật nguồn sau, nút Về tư thế chuẩn và mốc của các động tác. Khi chưa có góc lưu, chương trình dùng 90/90/90. Lần đầu chạy v4, chương trình chuyển góc đầu đã lưu cũ sang 90° một lần; giữ nguyên góc tay và hiệu chỉnh xích. Sau đó vẫn lưu được góc đầu mới qua popup.

## Xích và hiệu chỉnh

- Tốc độ nút di chuyển mặc định 45%; đổi trong popup.
- Thanh xích riêng điều khiển -100…100, duy trì đến khi về 0 hoặc bấm dừng.
- Xích trái mặc định dừng **1505 µs**, xích phải **1500 µs**. Bản v4 giữ hiệu chỉnh và chiều quay đã lưu từ v2.
- Áp dụng hoặc Đảo chiều gửi STOP trước, sau đó mới lưu cấu hình. Giao diện xác nhận kết quả.
- Tốc độ phần trăm là mức điều khiển, không phải RPM được đo bằng cảm biến.

## Chân giữ nguyên

| Servo | GPIO | Kênh LEDC |
|---|---:|---:|
| Xích trái 360° | 4 | 0 |
| Xích phải 360° | 5 | 1 |
| Đầu 180° | 6 | 2 |
| Tay trái 180° | 7 | 3 |
| Tay phải 180° | 15 | 4 |

LEDC 50 Hz, 14 bit. Dải điện của đầu là 1000–2000 µs; hai tay là 544–2400 µs, giữ theo các bài test riêng trước đó. Góc cơ khí thực tế phụ thuộc servo.

Nguồn servo phải phù hợp và GND chung với ESP32. Khi ESP được cấp USB để thử, tách +5 V nguồn servo khỏi VIN của ESP, giữ GND chung. Không đưa 5 V vào GPIO hoặc 3V3.

## Kết nối và dừng

- Một trình duyệt có quyền điều khiển; các trình duyệt khác ở chế độ xem.
- Xích dừng nếu không nhận lệnh chạy trong 700 ms.
- Động tác hủy nếu không nhận tín hiệu duy trì kết nối từ giao diện trong 1200 ms.
- Mất kết nối với trình duyệt điều khiển sẽ dừng xích và hủy động tác trước khi chuyển quyền.
- Rời trang hoặc chuyển ứng dụng sẽ gửi lệnh DỪNG; bộ đếm thời gian trong ESP xử lý trường hợp lệnh không tới được.
- Lệnh hướng, xích riêng hoặc góc thủ công hủy chuỗi đang chạy trước khi thực hiện lệnh mới. Chuỗi đã hủy không tự chạy lại.
- Mỗi nhịp xích tự dừng sau 200–280 ms, độc lập với thời gian tay/đầu đi đến góc đích. Nếu vòng lặp bị chậm bất thường, bộ đếm thời gian dừng khi vòng lặp được chạy lại.

## Chẩn đoán và kiểm tra

Serial Monitor 115200 in `BOOT Droid-E3D v4`, danh sách kênh, AP, ANGLE, TRACK và GESTURE. Trang **http://192.168.4.1/status** hiển thị xung phần mềm đã đặt, không phải phép đo vị trí hoặc tín hiệu thực tế.

Đã kiểm tra bằng C++ với API giả lập cho hai nhánh Arduino-ESP32 2.x/3.x: 12 động tác, cả 5 kênh trong chuỗi cảm xúc, thời hạn dừng xích độc lập, lệnh thủ công giành quyền, dừng/mất kết nối, chuyển góc đầu sang 90° một lần, lưu góc, giới hạn góc, tràn bộ đếm thời gian và xử lý lỗi đầu ra. JavaScript đã qua kiểm tra cú pháp và mô phỏng DOM/WebSocket, gồm hủy bộ gửi lệnh chạy tay khi bắt đầu cảm xúc.

Chưa biên dịch bằng toolchain ESP32 hoặc chạy trên robot thật; chưa dựng giao diện bằng trình duyệt thật trong môi trường này. Thử từng nút trên sàn có khoảng trống, vì bản này chủ động chạy xích. Hướng nâng/hạ tay phụ thuộc cách lắp thực tế.

## Chỉnh động tác trong code

Các bảng `CHEER`, `SAD`, `CURIOUS`, `GREET`, `SHOW` trong `.ino` chứa toàn bộ nhịp động tác. Mỗi hàng có cấu trúc:

```cpp
// mask, {đầu, tay trái, tay phải}, giữ ms, xích trái/phải, chạy xích ms, bước góc, chu kỳ bước ms
{7, {0,-40,40}, 160, 30,30,240, 2,20},
```

- `mask`: đầu=1, tay trái=2, tay phải=4; tổng 7 là cả ba khớp.
- Góc là độ lệch so với tư thế mặc định đã lưu, giới hạn 0–180°.
- Hai tốc độ xích: dương=tiến, âm=lùi, 0=dừng; vẫn dùng chiều quay và xung dừng đã hiệu chỉnh.
- Thời gian chạy xích tính từ lúc bắt đầu nhịp; thời gian giữ tính sau khi phần mềm đã đưa các khớp tới góc đích. Không có cảm biến xác nhận vị trí thực.
- Bước 2° mỗi 20 ms dùng cho động tác thường; Buồn dùng 1° mỗi 40 ms.
- Giữ nhịp xích dưới 700 ms để phù hợp bộ tự dừng hiện tại. Các preset dùng mức 22–32%, nhịp 200–280 ms.

Chuyển động xích là điều khiển theo thời gian, chưa có encoder; không đảm bảo tiến/lùi về đúng vị trí ban đầu hay xoay chính xác một số vòng. Các nút cảm xúc là chuỗi lập trình sẵn; nhận lệnh cảm xúc bằng giọng nói cần bước tích hợp XiaoZhi sau.

Phạm vi bản này là AP/WebSocket và chuyển động. XiaoZhi, màn hình, micro, amply, HC-SR04 chưa được tích hợp vào firmware này.
