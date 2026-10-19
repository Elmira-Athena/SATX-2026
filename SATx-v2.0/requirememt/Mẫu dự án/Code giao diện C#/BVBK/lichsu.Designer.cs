namespace BVBK
{
    partial class frmlichsu
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
			this.label1 = new System.Windows.Forms.Label();
			this.lvlichsu = new System.Windows.Forms.ListView();
			this.clnstt = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clnhovaten = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clngioitinh = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clncccd = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clnngaysinh = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clndiachi = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clncannang = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clnchieucao = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clnnhiptim = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clnoxy = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clnbmi = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.clnngaykham = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Font = new System.Drawing.Font("Times New Roman", 20.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.label1.ForeColor = System.Drawing.Color.Blue;
			this.label1.Location = new System.Drawing.Point(12, 9);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(297, 31);
			this.label1.TabIndex = 0;
			this.label1.Text = "LỊCH SỬ BỆNH NHÂN";
			// 
			// lvlichsu
			// 
			this.lvlichsu.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.clnstt,
            this.clnhovaten,
            this.clngioitinh,
            this.clncccd,
            this.clnngaysinh,
            this.clndiachi,
            this.clncannang,
            this.clnchieucao,
            this.clnnhiptim,
            this.clnoxy,
            this.clnbmi,
            this.clnngaykham});
			this.lvlichsu.HideSelection = false;
			this.lvlichsu.Location = new System.Drawing.Point(3, 43);
			this.lvlichsu.Name = "lvlichsu";
			this.lvlichsu.Size = new System.Drawing.Size(1022, 477);
			this.lvlichsu.TabIndex = 1;
			this.lvlichsu.UseCompatibleStateImageBehavior = false;
			this.lvlichsu.View = System.Windows.Forms.View.Details;
			// 
			// clnstt
			// 
			this.clnstt.Text = "STT";
			this.clnstt.Width = 36;
			// 
			// clnhovaten
			// 
			this.clnhovaten.Text = "Họ và tên";
			this.clnhovaten.Width = 178;
			// 
			// clngioitinh
			// 
			this.clngioitinh.Text = "Giới tính";
			// 
			// clncccd
			// 
			this.clncccd.Text = "CCCD";
			this.clncccd.Width = 120;
			// 
			// clnngaysinh
			// 
			this.clnngaysinh.Text = "Ngày sinh";
			this.clnngaysinh.Width = 111;
			// 
			// clndiachi
			// 
			this.clndiachi.Text = "Địa chỉ";
			this.clndiachi.Width = 117;
			// 
			// clncannang
			// 
			this.clncannang.Text = "Cân nặng";
			// 
			// clnchieucao
			// 
			this.clnchieucao.Text = "Chiều cao";
			this.clnchieucao.Width = 66;
			// 
			// clnnhiptim
			// 
			this.clnnhiptim.Text = "Nhịp tim";
			this.clnnhiptim.Width = 55;
			// 
			// clnoxy
			// 
			this.clnoxy.Text = "SPO2";
			this.clnoxy.Width = 49;
			// 
			// clnbmi
			// 
			this.clnbmi.Text = "BMI";
			this.clnbmi.Width = 48;
			// 
			// clnngaykham
			// 
			this.clnngaykham.Text = "Ngày khám";
			this.clnngaykham.Width = 149;
			// 
			// frmlichsu
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(1026, 532);
			this.Controls.Add(this.lvlichsu);
			this.Controls.Add(this.label1);
			this.Name = "frmlichsu";
			this.Text = "Lịch sử";
			this.Load += new System.EventHandler(this.frmlichsu_Load);
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.ListView lvlichsu;
        private System.Windows.Forms.ColumnHeader clnstt;
        private System.Windows.Forms.ColumnHeader clnhovaten;
        private System.Windows.Forms.ColumnHeader clncccd;
        private System.Windows.Forms.ColumnHeader clnngaysinh;
        private System.Windows.Forms.ColumnHeader clndiachi;
        private System.Windows.Forms.ColumnHeader clncannang;
        private System.Windows.Forms.ColumnHeader clnchieucao;
        private System.Windows.Forms.ColumnHeader clnnhiptim;
        private System.Windows.Forms.ColumnHeader clnoxy;
        private System.Windows.Forms.ColumnHeader clnbmi;
        private System.Windows.Forms.ColumnHeader clnngaykham;
        private System.Windows.Forms.ColumnHeader clngioitinh;
    }
}