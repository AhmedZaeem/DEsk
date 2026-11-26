import QtQuick
import calamares.slideshow 1.0

Presentation {
    id: presentation
    Timer { interval: 9000; running: presentation.activatedInCalamares; repeat: true; onTriggered: presentation.goToNextSlide() }
    Slide {
        Rectangle { anchors.fill: parent; color: "#140c0a" }
        Text { anchors.centerIn: parent; width: parent.width * 0.8; wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter; color: "#efe6d2"; font.pixelSize: 20
            text: "浪人 — Red Ronin\n\nA Japanese night study on DEsk: tiling as in Hyprland, a workspace on each screen, a storm outside the window." }
    }
    Slide {
        Rectangle { anchors.fill: parent; color: "#140c0a" }
        Text { anchors.centerIn: parent; width: parent.width * 0.8; wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter; color: "#efe6d2"; font.pixelSize: 18
            text: "Super+D or a tap of Super: the launcher.  Super+1…0: workspaces.  Super+Escape: power.\nPull the red cord at the top of the desk to open the window to the rain." }
    }
    Slide {
        Rectangle { anchors.fill: parent; color: "#140c0a" }
        Text { anchors.centerIn: parent; width: parent.width * 0.8; wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter; color: "#efe6d2"; font.pixelSize: 16
            text: "Made with AI: this fork of DEsk (by fyzugx) was edited with Claude, by Anthropic.\nWhat changed and why: REDRONIN.md." }
    }
}
