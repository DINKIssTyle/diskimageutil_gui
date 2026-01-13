package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"diskimageutil/core"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	fyneDialog "fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
	"github.com/sqweek/dialog"
)

func main() {
	// Set UI Scale here (0.5 to 3.0 usually)
	os.Setenv("FYNE_SCALE", "0.8")

	myApp := app.New()
	myWindow := myApp.NewWindow("DiskImageUtil")
	myWindow.Resize(fyne.NewSize(700, 500))

	var currentFile string

	// UI Elements
	title := canvas.NewText("DiskImageUtil", theme.PrimaryColor())
	title.TextSize = 24
	title.TextStyle = fyne.TextStyle{Bold: true}
	title.Alignment = fyne.TextAlignCenter

	fileLabel := widget.NewLabel("Please select a disk image file...")
	fileLabel.Alignment = fyne.TextAlignCenter

	// Use RichText for logs to have better contrast and styling
	logArea := widget.NewRichTextFromMarkdown("")
	logArea.Wrapping = fyne.TextWrapWord
	logScroll := container.NewScroll(logArea)

	progressBar := widget.NewProgressBar()
	progressBar.Hide()

	writableCheck := widget.NewCheck("Writable Image (for ISO/DMG)", nil)

	appendToLog := func(text string) {
		logArea.ParseMarkdown(logArea.String() + text + "\n")
		// Scroll to bottom
		logScroll.ScrollToBottom()
	}

	clearLog := func() {
		logArea.ParseMarkdown("")
	}

	// Actions
	selectFileBtn := widget.NewButtonWithIcon("Select Disk Image", theme.FileIcon(), func() {
		// Run native dialog in goroutine to not block Fyne UI
		go func() {
			filename, err := dialog.File().Title("Select Disk Image").Load()
			if err != nil {
				if err.Error() == "Cancelled" {
					return
				}
				// If native dialog fails/cancelled, just return or log
				// appendToLog("Dialog error: " + err.Error())
				return
			}

			currentFile = filename
			fileLabel.SetText(filepath.Base(currentFile))
			fileLabel.TextStyle = fyne.TextStyle{Bold: true}

			// Auto clear log on new file
			clearLog()
			appendToLog("**Selected:** `" + currentFile + "`")
		}()
	})

	infoBtn := widget.NewButtonWithIcon("Analyze Info", theme.InfoIcon(), func() {
		if currentFile == "" {
			// Using Fyne dialog for simple alerts is fine, simple and consistent
			fyneDialog.ShowInformation("Alert", "Please select a file first.", myWindow)
			return
		}
		clearLog()
		appendToLog("### Analyzing File...\n")

		// Run heavy work in goroutine
		go func() {
			desc, err := core.DescribeFile(currentFile, true)
			if err != nil {
				appendToLog("**Error:** " + err.Error())
				return
			}
			appendToLog("```\n" + desc + "\n```")
		}()
	})

	convertAction := func(iso bool) {
		if currentFile == "" {
			fyneDialog.ShowInformation("Alert", "Please select a file first.", myWindow)
			return
		}

		defaultName := currentFile
		ext := ".dsk"
		if iso {
			ext = ".iso"
		}
		defaultName = strings.TrimSuffix(defaultName, filepath.Ext(defaultName)) + ext

		go func() {
			outPath, err := dialog.File().Title("Save Output File").SetStartDir(filepath.Dir(defaultName)).Filter("Disk Image", strings.TrimPrefix(ext, ".")).Save()
			if err != nil {
				if err.Error() != "Cancelled" {
					appendToLog("\n**Save Error:** " + err.Error())
				}
				return
			}

			// Handle case where user didn't type extension? native dialog usually adds it if filter set, but let's be safe
			if !strings.HasSuffix(outPath, ext) {
				outPath += ext
			}

			clearLog()
			appendToLog(fmt.Sprintf("### Converting to %s...", filepath.Base(outPath)))
			progressBar.SetValue(0)
			progressBar.Show()

			err = core.ConvertFile(iso, currentFile, outPath, writableCheck.Checked, func(percent float64) {
				progressBar.SetValue(percent)
			})

			if err != nil {
				appendToLog("\n**Error:** " + err.Error())
				fyneDialog.ShowError(err, myWindow)
			} else {
				appendToLog("\n**Success!** File created at: `" + outPath + "`")
				fyneDialog.ShowInformation("Success", "Conversion Completed!", myWindow)
			}
			progressBar.Hide()
		}()
	}

	cvtHfsBtn := widget.NewButtonWithIcon("Convert to HFS (.dsk)", theme.StorageIcon(), func() {
		convertAction(false)
	})

	cvtIsoBtn := widget.NewButtonWithIcon("Convert to ISO (.iso)", theme.MediaPlayIcon(), func() {
		convertAction(true)
	})

	// Scale: Set default scale in code as requested
	// os.Setenv("FYNE_SCALE", "0.8") // Uncomment to force scale

	// Layout Grouping

	// Actions Container
	actions := container.NewVBox(
		container.NewPadded(selectFileBtn),
		container.NewCenter(fileLabel),
		container.NewPadded(writableCheck),
		container.NewGridWithColumns(3, infoBtn, cvtHfsBtn, cvtIsoBtn),
		container.NewPadded(progressBar),
	)

	// Combine Top Area
	topContent := container.NewVBox(
		container.NewPadded(title),
		actions,
		widget.NewSeparator(),
	)

	// Main Layout
	content := container.NewBorder(topContent, nil, nil, nil,
		container.NewPadded(logScroll),
	)

	myWindow.SetContent(content)
	myWindow.ShowAndRun()
}
