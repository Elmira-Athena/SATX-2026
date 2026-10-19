using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;

namespace BVBK
{
    public partial class frmlichsu : Form
    {
        public frmlichsu()
        {
            InitializeComponent();
        }
		public int LaySoDongHienTai()
		{
			return lvlichsu.Items.Count;
		}
		public void ThemVaoListView(string stt, string hovaten,string gioitinh, string cccd,string ngaySinh, string diachi, string cannang, string chieucao, string nhiptim, string spo2, string bmi, string thoigiankham)
		{
			// Tạo một dòng mới (ListViewItem) với giá trị đầu tiên là Họ tên
			ListViewItem item = new ListViewItem(stt);

			// Thêm các giá trị tiếp theo vào các cột con (SubItems)
			item.SubItems.Add(hovaten);
			item.SubItems.Add(gioitinh);
			item.SubItems.Add(cccd);
			item.SubItems.Add(ngaySinh);
			item.SubItems.Add(diachi);
			item.SubItems.Add(cannang);
			item.SubItems.Add(chieucao);
			item.SubItems.Add(nhiptim);
			item.SubItems.Add(spo2);
			item.SubItems.Add(bmi);
			item.SubItems.Add(thoigiankham);

			// Đưa dòng này vào ListView
			lvlichsu.Items.Add(item);
		}

		private void frmlichsu_Load(object sender, EventArgs e)
        {

        }
    }
}
