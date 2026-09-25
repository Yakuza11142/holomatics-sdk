pub mod text {
    use crate::core::{Canvas, Color, Scalar, Widget};

    #[derive(Debug, Clone, Copy, PartialEq)]
    pub enum TextAlignment {
        Left,
        Center,
        Right,
        Justified,
    }

    #[derive(Debug, Clone, PartialEq)]
    pub struct TextStyle {
        pub font_size: Scalar,
        pub color: Color,
        pub alignment: TextAlignment,
        pub line_height: Scalar,
    }

    impl Default for TextStyle {
        fn default() -> Self {
            Self {
                font_size: 16.0,
                color: Color::rgba(1.0, 1.0, 1.0, 1.0),
                alignment: TextAlignment::Left,
                line_height: 1.2,
            }
        }
    }

    #[derive(Debug, Clone, PartialEq)]
    pub struct Text {
        pub content: String,
        pub style: TextStyle,
    }

    impl Text {
        pub fn new(content: impl Into<String>, font_size: Scalar, color: Color) -> Self {
            Self {
                content: content.into(),
                style: TextStyle {
                    font_size,
                    color,
                    ..Default::default()
                },
            }
        }

        pub fn with_style(content: impl Into<String>, style: TextStyle) -> Self {
            Self {
                content: content.into(),
                style,
            }
        }
    }

    impl Widget for Text {
        fn render(&self, canvas: &mut Canvas) {
            if self.content.is_empty() {
                return;
            }

            canvas.draw_glyphs(
                &self.content,
                self.style.font_size,
                self.style.color,
                self.style.line_height,
            );
        }
    }
}
