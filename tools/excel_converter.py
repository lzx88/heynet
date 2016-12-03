import sys
import codecs
import xlrd
import os
import shutil

class ExcelConverter:
	def __init__(self, path, fname):
		self.fname = fname
		fullpath = os.path.join(path, fname)
		self.wbook = xlrd.open_workbook(fullpath)

	def getVal(self, val) :
		if type(val) == float :		
			val = str(val)
			ret = val.split(".")
			if len(ret) > 1 and ret[1] == "0" :
				val = ret[0]
		elif (type(val) == unicode or type(val) == str) :
			if val.find('[') != -1 and val.find(']') != -1 :
				pass
			else:
				val = u"\"" + val + u"\""
		return val

	def toLua(self, topath, mode, ext):
		for sht in self.wbook.sheets():
			sheet = self.wbook.sheet_by_name(sht.name)
			fullpath = os.path.join(topath, sht.name) + ext
			f = codecs.open(fullpath, "w", "utf-8")
			f.write("return ")
			f.write(u"{\n")
			for r in range(3, sheet.nrows):
				key = self.getVal(sheet.cell_value(r, 0))
				f.write(u"\t[%s] = {\n" % (key))
				for c in range(0, sheet.ncols):
					name = "[" + self.getVal(sheet.cell_value(2, c)) + "]"
					val = self.getVal(sheet.cell_value(r, c))
					desc = self.getVal(sheet.cell_value(1, c))
					flag = str.lower(str(sheet.cell_value(0, c)))
					if c == 0 or (mode == "s" and not flag == "c") or (mode == "c" and not flag == "s") :
						f.write(u"\t\t%-13s = %-13s--%s" % (name, val + u",", desc + u"\n"))
				f.write(u"\t},\n")
			f.write(u"}")
			f.close()
			print "Convert to " + fullpath + " success"

	def toJson(self, topath, mode, ext):
		for sht in self.wbook.sheets():
			sheet = self.wbook.sheet_by_name(sht.name)
			fullpath = os.path.join(topath, sht.name) + ext
			f = codecs.open(fullpath, "w", "utf-8")
			f.write(u"{\n")
			for r in range(3, sheet.nrows):
				key = self.getVal(sheet.cell_value(r, 0))
				f.write(u"\t\"%s\":{\n" % (key))
				for c in range(0, sheet.ncols):
					name = self.getVal(sheet.cell_value(2, c))
					val = self.getVal(sheet.cell_value(r, c))
					desc = self.getVal(sheet.cell_value(1, c))
					flag = str.lower(str(sheet.cell_value(0, c)))
					if c == 0 or (mode == "s" and not flag == "c") or (mode == "c" and not flag == "s") :
						f.write(u"\t\t%s:%s" % (name, val + u",\n"))
				f.write(u"\t},\n")	
			f.write(u"}")
			f.close()
			print "Convert to " + fullpath + " success"

def main():
	excel_root_path = sys.argv[1]
	target = sys.argv[2]
	topath = sys.argv[3]
	mode = sys.argv[4]
	ext = sys.argv[5]
	allxls = os.listdir(excel_root_path)
	for xlsname in allxls:
		if ".xlsx" not in xlsname:
			continue
		print(os.path.join(excel_root_path, xlsname) + " ...")
		converter = ExcelConverter(excel_root_path, xlsname)
		if target == "lua":
			converter.toLua(topath, mode, ext)
		elif target == "json":
			converter.toJson(topath, mode, ext)
if __name__=="__main__":
	main()