using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using iText.IO.Font;
using iText.IO.Font.Constants;
using iText.Kernel.Font;
using iText.Kernel.Pdf;
using iText.Layout;
using iText.Layout.Element;
using AForge.Video;
using AForge.Video.DirectShow;
using ZXing;

namespace BVBK
{
	public partial class frmtrangchu : Form
	{
		public frmtrangchu()
		{
			InitializeComponent();
		}
		private void Form1_Load(object sender, EventArgs e)
		{
			SetupWebcam();
			string[] ports = SerialPort.GetPortNames();

			
			if (ports.Length > 0)
			{
				try
				{
					
					serialPort1.PortName = ports[0];
					serialPort1.BaudRate = 9600;

					if (!serialPort1.IsOpen)
						serialPort1.Open();
					MessageBox.Show("Đã kết nối COM");

				}
				catch
				{
					
				}
			}
		}
		private string LayTinhTrangBMI(string bmiText)
		{
			if (double.TryParse(bmiText, out double bmi))
			{
				if (bmi < 18.6) return "Thiếu cân (Underweight)";
				if (bmi < 25) return "Bình thường (Normal)";
				if (bmi < 30) return "Thừa cân (Overweight)";
				if (bmi < 35) return "Béo phì độ I (Obese Class I)";
				return "Béo phì độ II (Obese Class II)";
			}
			return "Chưa xác định";
		}
		public void XuatFilePDF()
		{
			string path = Path.Combine(
				Environment.GetFolderPath(Environment.SpecialFolder.Desktop),
				txthovaten.Text + ".pdf");

			try
			{
				using (PdfWriter writer = new PdfWriter(path))
				using (PdfDocument pdf = new PdfDocument(writer))
				{
					Document document = new Document(pdf);

					string fontPath = Path.Combine(
						Environment.GetFolderPath(Environment.SpecialFolder.Fonts),
						"arial.ttf"
					);

					PdfFont font = PdfFontFactory.CreateFont(
						fontPath,
						PdfEncodings.IDENTITY_H,
						PdfFontFactory.EmbeddingStrategy.PREFER_EMBEDDED
					);

					document.SetFont(font);

					document.Add(new Paragraph("BỆNH VIỆN BÁCH KHOA"));
					document.Add(new Paragraph("------------------------------------------------"));

					document.Add(new Paragraph("HỌ TÊN: " + txthovaten.Text));
					document.Add(new Paragraph("GIỚI TÍNH: " + cboGioiTinh.Text));
					document.Add(new Paragraph("CCCD: " + txtcccd.Text));
					document.Add(new Paragraph("NGÀY SINH: " + dtpngaysinh.Value.ToString("dd/MM/yyyy")));
					document.Add(new Paragraph("ĐỊA CHỈ: " + txtdiachi.Text));

					document.Add(new Paragraph("CÂN NẶNG: " + txtcannang.Text + "kg"));
					document.Add(new Paragraph("CHIỀU CAO: " + txtchieucao.Text + "cm"));
					document.Add(new Paragraph("NHỊP TIM: " + txtnhiptim.Text + "bpm"));
					document.Add(new Paragraph("SPO2: " + txtspo2.Text + "%"));
					document.Add(new Paragraph("BMI: " + txtbmi.Text));
					string tinhTrang = LayTinhTrangBMI(txtbmi.Text);
					document.Add(new Paragraph("TÌNH TRẠNG CƠ THỂ: " + tinhTrang));
					document.Add(new Paragraph(""));
					document.Add(new Paragraph("NGÀY KHÁM: " + DateTime.Now.ToString("dd/MM/yyyy HH:mm")));

					document.Close();
				}

				Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
			}
			catch (Exception ex)
			{
				MessageBox.Show(ex.ToString());
			}
		}


		private void btnlammoi_Click(object sender, EventArgs e)
		{
			txthovaten.Text = "";
			cboGioiTinh.SelectedIndex = -1;
			txtdiachi.Text = "";
			txtcccd.Text = "";
			txtcannang.Text = "";
			txtchieucao.Text = "";
			txtspo2.Text = "";
			txtnhiptim.Text = "";
			txtbmi.Text = "";

			if (serialPort1.IsOpen)
			{
				serialPort1.Write("Lam moi\n"); 
			}

		}

		private void btnlichsu_Click(object sender, EventArgs e)
		{
			if (formLichSu != null && !formLichSu.IsDisposed)
			{
				formLichSu.Show(); 
				formLichSu.BringToFront();
			}
			else
			{
			
				formLichSu = new frmlichsu();
				formLichSu.Show();
			}
		}
		frmlichsu formLichSu = null;

		private void btnxuatpdf_Click(object sender, EventArgs e)
		{
			XuatFilePDF();
			if (formLichSu == null || formLichSu.IsDisposed)
			{
				formLichSu = new frmlichsu();
			}
			int soThuTuMoi = formLichSu.LaySoDongHienTai() + 1;
			string ngaySinh = dtpngaysinh.Value.ToString("dd/MM/yyyy");
			string thoiGianKham = DateTime.Now.ToString("dd/MM/yyyy HH:mm");
	
			formLichSu.ThemVaoListView(soThuTuMoi.ToString(), txthovaten.Text, cboGioiTinh.Text, txtcccd.Text, ngaySinh, txtdiachi.Text, txtcannang.Text, txtchieucao.Text, txtnhiptim.Text, txtspo2.Text, txtbmi.Text, thoiGianKham);
		}

		private void btnthoat_Click(object sender, EventArgs e)
		{
			DialogResult cc = MessageBox.Show(
				"Bạn có chắc muốn thoát không?",
				"Confirm",
				MessageBoxButtons.YesNo,
				MessageBoxIcon.Question);
			if (cc == DialogResult.Yes)
				Close();
		}

		private void btndo_Click(object sender, EventArgs e)
		{
			if (serialPort1.IsOpen)
			{
		
				serialPort1.Write("Do\n");
			}
			else
			{
				MessageBox.Show("Cổng COM chưa mở!");
			}
		}
		private void serialPort1_DataReceived(object sender, System.IO.Ports.SerialDataReceivedEventArgs e)
		{
			try
			{
				string dataFromPIC = serialPort1.ReadLine().Trim();

				if (this.IsDisposed || !this.IsHandleCreated) return;

				this.BeginInvoke(new MethodInvoker(() =>
				{
					if (dataFromPIC.StartsWith("H:"))
						txtchieucao.Text = dataFromPIC.Split(':')[1].Trim();

					else if (dataFromPIC.StartsWith("W:"))
						txtcannang.Text = dataFromPIC.Split(':')[1].Trim();

					else if (dataFromPIC.StartsWith("B:"))
						txtnhiptim.Text = dataFromPIC.Split(':')[1].Trim();

					else if (dataFromPIC.StartsWith("S:"))
						txtspo2.Text = dataFromPIC.Split(':')[1].Trim();

					else if (dataFromPIC.StartsWith("BMI:"))
						txtbmi.Text = dataFromPIC.Split(':')[1].Trim();
				}));
			}
			catch (TimeoutException)
			{
				
			}
			catch (Exception ex)
			{
				
				Debug.WriteLine("Serial error: " + ex.Message);
			}
		}
		private void Form1_FormClosing(object sender, FormClosingEventArgs e)
		{
			StopWebcam();
			if (serialPort1.IsOpen) serialPort1.Close();
		}
		private FilterInfoCollection captureDevices; 
		private VideoCaptureDevice videoSource;     

		
		private void SetupWebcam()
		{
			captureDevices = new FilterInfoCollection(FilterCategory.VideoInputDevice);
			if (captureDevices.Count > 0)
			{
				videoSource = new VideoCaptureDevice(captureDevices[0].MonikerString);
				videoSource.NewFrame += VideoSource_NewFrame;
			}
		}

		private void VideoSource_NewFrame(object sender, NewFrameEventArgs eventArgs)
		{
			Bitmap bitmap = null;
			try
			{
			
				bitmap = (Bitmap)eventArgs.Frame.Clone();

				
				this.Invoke(new MethodInvoker(delegate
				{
					
					System.Drawing.Image oldImage = pictureBoxWebcam.Image;

				
					pictureBoxWebcam.Image = (Bitmap)bitmap.Clone();

				
					if (oldImage != null)
					{
						oldImage.Dispose();
					}
				}));

		
				BarcodeReader reader = new BarcodeReader
				{
					AutoRotate = true,
					TryInverted = true,
					Options = new ZXing.Common.DecodingOptions
					{
						TryHarder = true,
						PossibleFormats = new List<BarcodeFormat>
		{
			BarcodeFormat.QR_CODE
		}
					}
				};

			
				float scaleX = (float)bitmap.Width / pictureBoxWebcam.Width;
				float scaleY = (float)bitmap.Height / pictureBoxWebcam.Height;

			
				int cropWidth = (int)(pictureBoxWebcam.Width / 2 * scaleX);
				int cropHeight = (int)(pictureBoxWebcam.Height / 2 * scaleY);

				int cropX = (bitmap.Width - cropWidth) / 2;
				int cropY = (bitmap.Height - cropHeight) / 2;

				Rectangle cropRect = new Rectangle(cropX, cropY, cropWidth, cropHeight);
				Bitmap cropped = bitmap.Clone(cropRect, bitmap.PixelFormat);

				
				Bitmap grayBitmap = new Bitmap(cropped.Width, cropped.Height);

				using (Graphics g = Graphics.FromImage(grayBitmap))
				{
					var colorMatrix = new System.Drawing.Imaging.ColorMatrix(
						new float[][]
						{
			new float[] {0.3f, 0.3f, 0.3f, 0, 0},
			new float[] {0.59f, 0.59f, 0.59f, 0, 0},
			new float[] {0.11f, 0.11f, 0.11f, 0, 0},
			new float[] {0, 0, 0, 1, 0},
			new float[] {0, 0, 0, 0, 1}
						});

					var attributes = new System.Drawing.Imaging.ImageAttributes();
					attributes.SetColorMatrix(colorMatrix);

					g.DrawImage(cropped,
						new Rectangle(0, 0, cropped.Width, cropped.Height),
						0, 0, cropped.Width, cropped.Height,
						GraphicsUnit.Pixel,
						attributes);
				}

			
				var result = reader.Decode(grayBitmap);

		
				cropped.Dispose();
				grayBitmap.Dispose();

				if (result != null && !string.IsNullOrEmpty(result.Text))
				{
					
					if (videoSource != null && videoSource.IsRunning)
					{
						videoSource.SignalToStop();
					}

				
					this.Invoke(new MethodInvoker(delegate
					{
						
						pictureBoxWebcam.Image = null;

						
						string data = result.Text;
						MessageBox.Show("Đã đọc được mã: " + data);

						
						ParseCCCD(data);
					}));
				}
			}
			catch (Exception ex)
			{
				Console.WriteLine("Lỗi: " + ex.Message);
			}
			finally
			{
				if (bitmap != null) bitmap.Dispose();
			}
		}

		private void ParseCCCD(string data)
		{
			
			string[] parts = data.Split('|');
			if (parts.Length >= 6)
			{
				txtcccd.Text = parts[0]; 
				txthovaten.Text = parts[2]; 
				string gender = parts[4]; 
				cboGioiTinh.Text = gender;

			
				string dob = parts[3];
				if (dob.Length == 8)
					dtpngaysinh.Value = new DateTime(int.Parse(dob.Substring(4, 4)), int.Parse(dob.Substring(2, 2)), int.Parse(dob.Substring(0, 2)));

				txtdiachi.Text = parts[5];
				MessageBox.Show("Đã quét thành công thông tin CCCD!");
			}
		}

		private void btnQuetMa_Click(object sender, EventArgs e)
		{
			if (videoSource != null && !videoSource.IsRunning)
			{
				videoSource.Start();
			}
		}

		private void StopWebcam()
		{
			if (videoSource != null && videoSource.IsRunning)
			{
				videoSource.SignalToStop();
				pictureBoxWebcam.Image = null;
			}
		}
		private void pictureBoxWebcam_Paint(object sender, PaintEventArgs e)
		{
			
			using (Pen pen = new Pen(Color.LimeGreen, 4))
			{
				int width = pictureBoxWebcam.Width / 2;  
				int height = pictureBoxWebcam.Height / 2; 
				int x = (pictureBoxWebcam.Width - width) / 2;
				int y = (pictureBoxWebcam.Height - height) / 2;

			
				e.Graphics.DrawRectangle(pen, x, y, width, height);

			
			}

			using (Font font = new Font("Arial", 10, FontStyle.Bold))
			{
				e.Graphics.DrawString("ĐƯA MÃ QR VÀO KHUNG NÀY", font, Brushes.LimeGreen, 10, 10);
			}
		}
	}
}